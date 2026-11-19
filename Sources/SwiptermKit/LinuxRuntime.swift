import CSwiptermKit
import Foundation
import Network

public final class LinuxRuntime: @unchecked Sendable {
    public struct Configuration: Sendable {
        public enum InitMode: Sendable {
            case managed
            case process(
                executablePath: String,
                arguments: [String] = [],
                environment: [String: String]? = nil,
                consoleOutput: (@Sendable (Data) -> Void)? = nil
            )
        }

        public var root: URL
        public var initialImage: LinuxRootImage?
        public var installationProgress: (@Sendable (Double) -> Void)?
        public var initMode: InitMode
        public var hostname: String?
        public var version: String?
        public var hostCommandsDirectory: String?
        public var autoSyncDNS: Bool
        public var panicHandler: (@Sendable (String) -> Void)?
        public var haltHandler: (@Sendable () -> Void)?

        public init(
            root: URL,
            initialImage: LinuxRootImage? = nil,
            installationProgress: (@Sendable (Double) -> Void)? = nil,
            initMode: InitMode = .managed,
            hostname: String? = nil,
            version: String? = nil,
            hostCommandsDirectory: String? = "/run/host-bin",
            autoSyncDNS: Bool = true,
            panicHandler: (@Sendable (String) -> Void)? = nil,
            haltHandler: (@Sendable () -> Void)? = nil
        ) {
            self.root = root
            self.initialImage = initialImage
            self.installationProgress = installationProgress
            self.initMode = initMode
            self.hostname = hostname
            self.version = version
            self.hostCommandsDirectory = hostCommandsDirectory
            self.autoSyncDNS = autoSyncDNS
            self.panicHandler = panicHandler
            self.haltHandler = haltHandler
        }
    }

    public let defaultEnvironment: [String: String]
    public let fileSystem = LinuxFileSystem()
    private var pathMonitor: NWPathMonitor?

    private init(configuration: Configuration) {
        var path = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
        if let directory = configuration.hostCommandsDirectory {
            path += ":" + directory
        }
        defaultEnvironment = [
            "HOME": "/root",
            "PATH": path,
            "TERM": "xterm-256color",
        ]
    }

    /// `prepare` runs once the kernel is up and before the first guest process starts.
    public static func boot(
        _ configuration: Configuration,
        prepare: (LinuxRuntime) async throws -> Void = { _ in }
    ) async throws -> LinuxRuntime {
        let dataDirectory = configuration.root.appending(path: "data", directoryHint: .isDirectory)
        if !FileManager.default.fileExists(atPath: dataDirectory.path(percentEncoded: false)) {
            guard let image = configuration.initialImage else {
                throw LinuxError.ENOENT
            }
            try await image.install(to: configuration.root, progress: configuration.installationProgress)
        }

        let runtime = LinuxRuntime(configuration: configuration)
        let initCommand: (path: String, arguments: [String], environment: [String])?
        let consoleOutput: (@Sendable (Data) -> Void)?
        switch configuration.initMode {
        case .managed:
            initCommand = nil
            consoleOutput = nil
        case .process(let path, let arguments, let environment, let output):
            let environment = (environment ?? runtime.defaultEnvironment).sorted { $0.key < $1.key }
            initCommand = (path, [path] + arguments, environment.map { "\($0.key)=\($0.value)" })
            consoleOutput = output
        }
        let callbacks = RuntimeCallbacks(
            consoleOutput: consoleOutput,
            haltHandler: configuration.haltHandler,
            panicHandler: configuration.panicHandler
        )
        try await RuntimeCallbacks.boot(callbacks) {
            try await blocking {
                try bootKernel(configuration)
            }
        }

        if configuration.autoSyncDNS {
            try? runtime.syncDNS()
            let monitor = NWPathMonitor()
            monitor.pathUpdateHandler = { [weak runtime] _ in
                try? runtime?.syncDNS()
            }
            monitor.start(queue: DispatchQueue(label: "SwiptermKit.PathMonitor"))
            runtime.pathMonitor = monitor
        }
        try await prepare(runtime)
        try await blocking {
            try startInit(initCommand)
        }
        return runtime
    }

    private static func startInit(
        _ command: (path: String, arguments: [String], environment: [String])?
    ) throws(LinuxError) {
        let arguments = command?.arguments ?? []
        let code = withCStringArray(arguments) { argv in
            withCStringArray(command?.environment ?? []) { envp in
                withOptionalCString(command?.path) { path in
                    swipterm_start_init(path, arguments.count, argv, envp)
                }
            }
        }
        try check(code)
    }

    private static func bootKernel(_ configuration: Configuration) throws(LinuxError) {
        #if targetEnvironment(simulator)
            let socketPrefix: String? = nil
        #else
            let socketPrefix: String? = URL.temporaryDirectory.appending(path: "ishsock").path(percentEncoded: false)
        #endif
        var callbackTable = RuntimeCallbacks.table
        if configuration.panicHandler == nil {
            callbackTable.panic = nil
        }
        if case .managed = configuration.initMode {
            callbackTable.console_output = nil
        } else if case .process(_, _, _, nil) = configuration.initMode {
            callbackTable.console_output = nil
        }
        let code = configuration.root.path(percentEncoded: false).withCString { root in
            withOptionalCString(socketPrefix) { socketPrefix in
                withOptionalCString(configuration.hostname) { hostname in
                    withOptionalCString(configuration.version) { version in
                        withOptionalCString(configuration.hostCommandsDirectory) { hostCommandsDirectory in
                            var options = swipterm_options(
                                root_path: root,
                                socket_prefix: socketPrefix,
                                hostname: hostname,
                                version: version,
                                host_commands_directory: hostCommandsDirectory,
                                callbacks: callbackTable
                            )
                            return swipterm_boot(&options)
                        }
                    }
                }
            }
        }
        try check(code)
    }

    public func suspend() {
        swipterm_suspend()
    }

    public func resume() {
        swipterm_resume()
        try? syncDNS()
    }

    public func syncDNS() throws(LinuxError) {
        try check(swipterm_sync_dns())
    }

    public func makeTerminal(size: LinuxTerminal.Size) throws(LinuxError) -> LinuxTerminal {
        try LinuxTerminal(size: size)
    }

    public func spawn(_ configuration: LinuxProcess.Configuration) async throws -> LinuxProcess {
        try await LinuxProcess(configuration, environment: configuration.environment ?? defaultEnvironment)
    }

    public func run(_ configuration: LinuxProcess.Configuration, input: Data? = nil) async throws -> LinuxProcess.Output {
        var configuration = configuration
        configuration.standardInput = input != nil ? .pipe : .null
        configuration.standardOutput = .pipe
        configuration.standardError = .pipe
        let process = try await spawn(configuration)
        async let output = process.standardOutput!.readToEnd()
        async let error = process.standardError!.readToEnd()
        if let input, let standardInput = process.standardInput {
            try? await standardInput.write(input)
            standardInput.close()
        }
        return try await LinuxProcess.Output(
            termination: process.termination,
            standardOutput: output,
            standardError: error
        )
    }

    public func mount(_ hostDirectory: URL, at guestPath: String, readOnly: Bool = false) async throws {
        let hostPath = hostDirectory.path(percentEncoded: false)
        try await blocking {
            try check(swipterm_mount(hostPath, guestPath, readOnly))
        }
    }

    public func unmount(at guestPath: String) async throws {
        try await blocking {
            try check(swipterm_unmount(guestPath))
        }
    }

    /// Guest `execve` of `hostCommandsDirectory`/`name` runs `handler` instead of a guest program, and the
    /// process exits with the returned status. SIGHUP, SIGINT, SIGQUIT, SIGTERM and SIGKILL cancel the
    /// handler's task and terminate the process by that signal. Guest standard input is forwarded while
    /// the command runs, including input typed on a terminal.
    public func registerCommand(
        named name: String,
        handler: @escaping @Sendable (HostCommandInvocation) async throws -> Int32
    ) async throws {
        let command = Unmanaged.passRetained(CommandBox(name: name, handler: handler))
        let context = UnsafeSendable(value: command.toOpaque())
        do {
            try await blocking {
                try check(swipterm_register_command(name, context.value))
            }
        } catch {
            command.release()
            throw error
        }
    }

    public func registerDevice(at path: String, handler: some LinuxCharacterDevice) async throws {
        let device = Unmanaged.passRetained(DeviceBox(device: handler))
        let context = UnsafeSendable(value: device.toOpaque())
        do {
            try await blocking {
                try check(swipterm_register_device(path, context.value))
            }
        } catch {
            device.release()
            throw error
        }
    }
}
