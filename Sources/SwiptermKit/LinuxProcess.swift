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
    private let handle: OpaquePointer
    private let state: State
    private let terminals: [LinuxTerminal]

    init(_ configuration: Configuration, environment: [String: String]) async throws {
        let state = State()
        let terminals = [configuration.standardInput, configuration.standardOutput, configuration.standardError]
            .compactMap { io -> LinuxTerminal? in
                if case .terminal(let terminal) = io { terminal } else { nil }
            }
        let context = Unmanaged.passRetained(state)
        let spawned: (handle: UnsafeSendable<OpaquePointer>, pid: Int32, descriptors: [Int32])
        do {
            spawned = try await blocking {
                try Self.spawn(configuration, environment: environment, context: UnsafeSendable(value: context.toOpaque()))
            }
        } catch {
            context.release()
            throw error
        }
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
        _ configuration: Configuration,
        environment: [String: String],
        context: UnsafeSendable<UnsafeMutableRawPointer>
    ) throws -> (handle: UnsafeSendable<OpaquePointer>, pid: Int32, descriptors: [Int32]) {
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
                            context: context.value
                        )
                        return descriptors.withUnsafeMutableBufferPointer {
                            swipterm_spawn(&options, &handle, &pid, $0.baseAddress)
                        }
                    }
                }
            }
        }
        try check(code)
        return (UnsafeSendable(value: handle!), pid, descriptors)
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
