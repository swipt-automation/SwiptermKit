import CSwiptermKit
import Foundation

public final class LinuxProcess: @unchecked Sendable {
    public struct Configuration: Sendable {
        let executablePath: String
        let arguments: [String]
        let environment: [String: String]
        let workingDirectory: String?
        let standardInput: StandardIO
        let standardOutput: StandardIO
        let standardError: StandardIO

        public init(
            executablePath: String,
            arguments: [String] = [],
            environment: [String: String],
            workingDirectory: String? = nil,
            standardInput: StandardIO = .null,
            standardOutput: StandardIO = .null,
            standardError: StandardIO = .null
        ) {
            self.executablePath = executablePath
            self.arguments = arguments
            self.environment = environment
            self.workingDirectory = workingDirectory
            self.standardInput = standardInput
            self.standardOutput = standardOutput
            self.standardError = standardError
        }
    }

    public enum StandardIO: Sendable {
        case null
        case terminal(LinuxTerminal)
    }

    public enum Termination: Hashable, Sendable {
        case exited(Int32)
        case signaled(LinuxSignal)

        init(status: Int32) {
            let signal = status & 0x7F
            self = signal != 0 ? .signaled(LinuxSignal(rawValue: signal)) : .exited((status >> 8) & 0xFF)
        }
    }

    final class State: @unchecked Sendable {
        private let lock = NSLock()
        private var termination: Termination?
        private var waiters: [CheckedContinuation<Termination, Never>] = []

        func exit(status: Int32) {
            let termination = Termination(status: status)
            let waiters = lock.withLock {
                self.termination = termination
                defer { self.waiters.removeAll() }
                return self.waiters
            }
            for waiter in waiters {
                waiter.resume(returning: termination)
            }
        }

        func wait() async -> Termination {
            await withCheckedContinuation { continuation in
                let termination = lock.withLock {
                    if self.termination == nil {
                        waiters.append(continuation)
                    }
                    return self.termination
                }
                if let termination {
                    continuation.resume(returning: termination)
                }
            }
        }
    }

    private let runtime: LinuxRuntime
    private let handle: OpaquePointer
    private let state: State
    private let terminals: [LinuxTerminal]

    private struct Launch: Sendable {
        let handle: UnsafeSendable<OpaquePointer>
        let input: Int32
    }

    convenience init(
        runtime: LinuxRuntime,
        disk: UInt32,
        _ configuration: Configuration,
        hostname: String,
        shell: Bool,
        invocation: HostCommandInvocation?
    ) async throws {
        let terminals = invocation != nil ? [] : [
            configuration.standardInput, configuration.standardOutput, configuration.standardError,
        ].compactMap { io -> LinuxTerminal? in
            if case let .terminal(terminal) = io {
                terminal
            } else {
                nil
            }
        }
        let invocationHandle = invocation?.handle
        try await self.init(runtime: runtime, terminals: terminals, forwarding: invocation?.standardInput) { runtimeHandle, context in
            func stdio(_ io: StandardIO) -> swipterm_stdio {
                switch io {
                case .null:
                    swipterm_stdio(kind: SWIPTERM_STDIO_NULL, terminal: nil)
                case let .terminal(terminal):
                    swipterm_stdio(kind: SWIPTERM_STDIO_TERMINAL, terminal: terminal.handle)
                }
            }
            let arguments = [configuration.executablePath] + configuration.arguments
            let environment = configuration.environment.sorted { $0.key < $1.key }.map { "\($0.key)=\($0.value)" }
            var handle: OpaquePointer?
            var input: Int32 = -1
            let code = withCStringArray(arguments) { argv in
                withCStringArray(environment) { envp in
                    configuration.executablePath.withCString { path in
                        withOptionalCString(configuration.workingDirectory) { workingDirectory in
                            hostname.withCString { hostname in
                                var options = swipterm_spawn_options(
                                    path: path,
                                    argc: arguments.count,
                                    argv: argv,
                                    envp: envp,
                                    working_directory: workingDirectory,
                                    stdio: (
                                        stdio(configuration.standardInput),
                                        stdio(configuration.standardOutput),
                                        stdio(configuration.standardError)
                                    ),
                                    context: context.value,
                                    disk: disk,
                                    hostname: hostname,
                                    shell: shell,
                                    stdio_invocation: invocationHandle?.value
                                )
                                return swipterm_spawn(runtimeHandle, &options, &handle, &input)
                            }
                        }
                    }
                }
            }
            try check(code)
            return Launch(handle: UnsafeSendable(value: handle!), input: input)
        }
    }

    convenience init(
        runtime: LinuxRuntime,
        loginAs user: String,
        on terminal: LinuxTerminal,
        environment: [String: String]
    ) async throws {
        let environment = environment.sorted { $0.key < $1.key }.map { "\($0.key)=\($0.value)" }
        let terminalHandle = UnsafeSendable(value: terminal.handle)
        try await self.init(runtime: runtime, terminals: [terminal], forwarding: nil) { runtimeHandle, context in
            var handle: OpaquePointer?
            let code = withCStringArray(environment) { envp in
                user.withCString { user in
                    var options = swipterm_login_options(
                        terminal: terminalHandle.value,
                        user: user,
                        envp: envp,
                        context: context.value
                    )
                    return swipterm_login(runtimeHandle, &options, &handle)
                }
            }
            try check(code)
            return Launch(handle: UnsafeSendable(value: handle!), input: -1)
        }
    }

    private init(
        runtime: LinuxRuntime,
        terminals: [LinuxTerminal],
        forwarding source: LinuxPipeReader?,
        launch: @escaping @Sendable (OpaquePointer, UnsafeSendable<UnsafeMutableRawPointer>) throws -> Launch
    ) async throws {
        let state = State()
        let context = Unmanaged.passRetained(state)
        let spawned: Launch
        do {
            let contextPointer = UnsafeSendable(value: context.toOpaque())
            spawned = try await runtime.withActiveHandleBlocking { runtimeHandle in
                try launch(runtimeHandle, contextPointer)
            }
        } catch {
            context.release()
            throw error
        }
        self.runtime = runtime
        self.state = state
        self.terminals = terminals
        handle = spawned.handle.value
        if let source, spawned.input >= 0 {
            let input = LinuxPipeWriter(fileDescriptor: spawned.input)
            Task {
                while let chunk = try? await source.read() {
                    guard (try? await input.write(chunk)) != nil else { break }
                }
                input.close()
            }
        }
    }

    deinit {
        swipterm_process_release(handle)
    }

    public var termination: Termination {
        get async {
            await state.wait()
        }
    }

    public func send(_ signal: LinuxSignal) throws(LinuxError) {
        try check(swipterm_process_signal(handle, signal.rawValue))
    }
}
