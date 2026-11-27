import CSwiptermKit
import Foundation

public final class LinuxProcess: @unchecked Sendable {
    public struct Configuration: Sendable {
        public var executablePath: String
        public var arguments: [String]
        public var environment: [String: String]?
        public var workingDirectory: String?
        public var standardInput: StandardIO
        public var standardOutput: StandardIO
        public var standardError: StandardIO

        public init(
            executablePath: String,
            arguments: [String] = [],
            environment: [String: String]? = nil,
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
        case pipe
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

    public struct Output: Sendable {
        public var termination: Termination
        public var standardOutput: Data
        public var standardError: Data
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

    public let pid: Int32
    public let standardInput: LinuxPipeWriter?
    public let standardOutput: LinuxPipeReader?
    public let standardError: LinuxPipeReader?
    private let runtime: LinuxRuntime
    private let handle: OpaquePointer
    private let state: State
    private let terminals: [LinuxTerminal]

    private struct Launch: Sendable {
        let handle: UnsafeSendable<OpaquePointer>
        let pid: Int32
        let descriptors: [Int32]
    }

    struct Target: Sendable {
        var disk: UInt32 = 0
        var hostname: String?
        var shell = false
        var invocation: UnsafeSendable<OpaquePointer>?
    }

    convenience init(
        runtime: LinuxRuntime,
        _ configuration: Configuration,
        environment: [String: String],
        target: Target = Target()
    ) async throws {
        let terminals = target.invocation != nil ? [] : [
            configuration.standardInput, configuration.standardOutput, configuration.standardError,
        ].compactMap { io -> LinuxTerminal? in
            if case .terminal(let terminal) = io { terminal } else { nil }
        }
        try await self.init(runtime: runtime, terminals: terminals) { runtimeHandle, context in
            try Self.spawn(
                runtimeHandle: runtimeHandle,
                configuration,
                environment: environment,
                target: target,
                context: context
            )
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
        try await self.init(runtime: runtime, terminals: [terminal]) { runtimeHandle, context in
            var handle: OpaquePointer?
            var pid: Int32 = 0
            let code = withCStringArray(environment) { envp in
                user.withCString { user in
                    var options = swipterm_login_options(
                        terminal: terminalHandle.value,
                        user: user,
                        envp: envp,
                        context: context.value
                    )
                    return swipterm_login(runtimeHandle, &options, &handle, &pid)
                }
            }
            try check(code)
            return Launch(handle: UnsafeSendable(value: handle!), pid: pid, descriptors: [-1, -1, -1])
        }
    }

    private init(
        runtime: LinuxRuntime,
        terminals: [LinuxTerminal],
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
        pid = spawned.pid
        standardInput = spawned.descriptors[0] >= 0 ? LinuxPipeWriter(fileDescriptor: spawned.descriptors[0]) : nil
        standardOutput = spawned.descriptors[1] >= 0 ? LinuxPipeReader(fileDescriptor: spawned.descriptors[1]) : nil
        standardError = spawned.descriptors[2] >= 0 ? LinuxPipeReader(fileDescriptor: spawned.descriptors[2]) : nil
    }

    deinit {
        swipterm_process_release(handle)
    }

    private static func spawn(
        runtimeHandle: OpaquePointer,
        _ configuration: Configuration,
        environment: [String: String],
        target: Target,
        context: UnsafeSendable<UnsafeMutableRawPointer>
    ) throws -> Launch {
        func stdio(_ io: StandardIO) -> swipterm_stdio {
            switch io {
            case .null:
                swipterm_stdio(kind: SWIPTERM_STDIO_NULL, terminal: nil)
            case .pipe:
                swipterm_stdio(kind: SWIPTERM_STDIO_PIPE, terminal: nil)
            case .terminal(let terminal):
                swipterm_stdio(kind: SWIPTERM_STDIO_TERMINAL, terminal: terminal.handle)
            }
        }
        let arguments = [configuration.executablePath] + configuration.arguments
        let environment = environment.sorted { $0.key < $1.key }.map { "\($0.key)=\($0.value)" }
        var handle: OpaquePointer?
        var pid: Int32 = 0
        var descriptors: [Int32] = [-1, -1, -1]
        let code = withCStringArray(arguments) { argv in
            withCStringArray(environment) { envp in
                configuration.executablePath.withCString { path in
                    withOptionalCString(configuration.workingDirectory) { workingDirectory in
                        withOptionalCString(target.hostname) { hostname in
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
                                disk: target.disk,
                                hostname: hostname,
                                shell: target.shell,
                                stdio_invocation: target.invocation?.value
                            )
                            return descriptors.withUnsafeMutableBufferPointer {
                                swipterm_spawn(runtimeHandle, &options, &handle, &pid, $0.baseAddress)
                            }
                        }
                    }
                }
            }
        }
        try check(code)
        return Launch(handle: UnsafeSendable(value: handle!), pid: pid, descriptors: descriptors)
    }

    public var termination: Termination {
        get async {
            await state.wait()
        }
    }

    public func send(_ signal: LinuxSignal, toProcessGroup: Bool = false) throws(LinuxError) {
        try check(swipterm_process_signal(handle, signal.rawValue, toProcessGroup))
    }
}

func withOptionalCString<Result>(_ string: String?, _ body: (UnsafePointer<CChar>?) throws -> Result) rethrows -> Result {
    guard let string else {
        return try body(nil)
    }
    return try string.withCString(body)
}
