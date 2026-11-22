import CSwiptermKit
import Foundation
import Network

public enum LinuxBlobSource: Sendable, Equatable {
    case embedded
    case file(URL)
    case data(Data)

    func withCBlob<Result>(_ body: (swipterm_blob) throws -> Result) rethrows -> Result {
        switch self {
        case .embedded:
            return try body(swipterm_blob(kind: SWIPTERM_BLOB_EMBEDDED, path: nil, data: nil, size: 0))
        case .file(let url):
            return try url.withUnsafeFileSystemRepresentation { path in
                try body(swipterm_blob(kind: SWIPTERM_BLOB_FILE, path: path, data: nil, size: 0))
            }
        case .data(let data):
            return try data.withUnsafeBytes { buffer in
                try body(swipterm_blob(
                    kind: SWIPTERM_BLOB_MEMORY,
                    path: nil,
                    data: buffer.baseAddress,
                    size: buffer.count
                ))
            }
        }
    }
}

public final class LinuxRuntime: @unchecked Sendable {
    public struct Configuration: Sendable {
        public enum InitMode: Hashable, Sendable {
            /// The root's `/sbin/init` runs as PID 1 of a system container.
            case system
            /// No guest init runs; the runtime supervises the processes it starts.
            case managed
        }

        public var root: URL
        public var initialImage: LinuxRootImage?
        public var installationProgress: (@Sendable (Double) -> Void)?
        public var initMode: InitMode
        public var hostname: String?
        public var hostCommandsDirectory: String?
        public var autoSyncDNS: Bool
        public var kernelImage: LinuxBlobSource
        public var initramfs: LinuxBlobSource
        public var modulesInitramfs: LinuxBlobSource?
        public var extraKernelArguments: String?
        public var consoleOutput: (@Sendable (Data) -> Void)?
        public var panicHandler: (@Sendable (String) -> Void)?
        public var haltHandler: (@Sendable (HaltReason) -> Void)?

        public init(
            root: URL,
            initialImage: LinuxRootImage? = nil,
            installationProgress: (@Sendable (Double) -> Void)? = nil,
            initMode: InitMode = .system,
            hostname: String? = nil,
            hostCommandsDirectory: String? = "/run/host-bin",
            autoSyncDNS: Bool = true,
            kernelImage: LinuxBlobSource = .embedded,
            initramfs: LinuxBlobSource = .embedded,
            modulesInitramfs: LinuxBlobSource? = nil,
            extraKernelArguments: String? = nil,
            consoleOutput: (@Sendable (Data) -> Void)? = nil,
            panicHandler: (@Sendable (String) -> Void)? = nil,
            haltHandler: (@Sendable (HaltReason) -> Void)? = nil
        ) {
            self.root = root
            self.initialImage = initialImage
            self.installationProgress = installationProgress
            self.initMode = initMode
            self.hostname = hostname
            self.hostCommandsDirectory = hostCommandsDirectory
            self.autoSyncDNS = autoSyncDNS
            self.kernelImage = kernelImage
            self.initramfs = initramfs
            self.modulesInitramfs = modulesInitramfs
            self.extraKernelArguments = extraKernelArguments
            self.consoleOutput = consoleOutput
            self.panicHandler = panicHandler
            self.haltHandler = haltHandler
        }
    }

    public enum Readiness: Hashable, Sendable {
        /// A root login can be opened.
        case login
        /// The guest init finished starting the system.
        case system
    }

    /// The machine has stopped in every case; `reboot` reports that the guest asked to restart.
    public enum HaltReason: Hashable, Sendable {
        case reboot
        case powerOff
        /// The guest init exited; `nil` when the machine stopped before the init reported its status.
        case exited(LinuxProcess.Termination?)

        init(_ reason: swipterm_halt_reason, waitStatus: Int32) {
            switch reason {
            case SWIPTERM_HALT_REBOOT:
                self = .reboot
            case SWIPTERM_HALT_POWER_OFF:
                self = .powerOff
            case SWIPTERM_HALT_EXIT:
                self = .exited(LinuxProcess.Termination(status: waitStatus))
            default:
                self = .exited(nil)
            }
        }
    }

    public let defaultEnvironment: [String: String]
    public var fileSystem: LinuxFileSystem {
        LinuxFileSystem(runtime: self)
    }

    let handle: OpaquePointer
    private let callbacks: RuntimeCallbacks
    private let stateCondition = NSCondition()
    private var pathMonitor: NWPathMonitor?
    private var registeredCommands: [Unmanaged<CommandBox>] = []
    private var registeredDevices: [Unmanaged<DeviceBox>] = []
    private var activeOperations = 0
    private var isShutDown = false
    private var isShutdownComplete = false

    private static let systemPath = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
    private static let readinessPollInterval = Duration.milliseconds(250)

    private init(configuration: Configuration, handle: OpaquePointer, callbacks: RuntimeCallbacks) {
        var path = Self.systemPath
        if let directory = configuration.hostCommandsDirectory {
            path += ":" + directory
        }
        defaultEnvironment = [
            "HOME": "/root",
            "PATH": path,
            "TERM": "xterm-256color",
        ]
        self.handle = handle
        self.callbacks = callbacks
    }

    deinit {
        guard !isShutDown else {
            return
        }
        let teardown = UnsafeSendable(value: (
            handle: handle,
            callbacks: callbacks,
            monitor: pathMonitor,
            commands: registeredCommands,
            devices: registeredDevices
        ))
        DispatchQueue.global(qos: .utility).async {
            let state = teardown.value
            state.monitor?.cancel()
            withExtendedLifetime(state.callbacks) {
                swipterm_shutdown(state.handle)
            }
            Self.release(commands: state.commands, devices: state.devices)
        }
    }

    /// Stops the guest init, waits for the guest to unmount its storage, and powers the machine off.
    /// Blocks the calling thread until the machine has stopped, and does not call `haltHandler`.
    /// Releasing the last reference without calling it performs the same shutdown on a background thread.
    public func shutdown() {
        let state: (Bool, NWPathMonitor?) = stateCondition.withLock {
            if isShutDown {
                while !isShutdownComplete {
                    stateCondition.wait()
                }
                return (false, nil)
            }
            isShutDown = true
            let monitor = pathMonitor
            pathMonitor = nil
            return (true, monitor)
        }
        guard state.0 else {
            return
        }
        state.1?.cancel()
        swipterm_begin_shutdown(handle)
        let (commands, devices): ([Unmanaged<CommandBox>], [Unmanaged<DeviceBox>]) = stateCondition.withLock {
            while activeOperations > 0 {
                stateCondition.wait()
            }
            let commands = registeredCommands
            registeredCommands.removeAll()
            let devices = registeredDevices
            registeredDevices.removeAll()
            return (commands, devices)
        }
        swipterm_shutdown(handle)
        Self.release(commands: commands, devices: devices)
        stateCondition.withLock {
            isShutdownComplete = true
            stateCondition.broadcast()
        }
    }

    private static func release(commands: [Unmanaged<CommandBox>], devices: [Unmanaged<DeviceBox>]) {
        for command in commands {
            command.release()
        }
        for device in devices {
            device.release()
        }
    }

    /// `prepare` runs once the kernel is up and before the first guest process starts.
    public static func boot(
        _ configuration: Configuration,
        prepare: @Sendable (LinuxRuntime) async throws -> Void = { _ in }
    ) async throws -> LinuxRuntime {
        let metadataFile = configuration.root.appending(path: "swipterm.json")
        let upperImage = configuration.root.appending(path: "upper.ext4")
        let fm = FileManager.default
        let hasRoot = fm.fileExists(atPath: metadataFile.path(percentEncoded: false)) &&
            fm.fileExists(atPath: upperImage.path(percentEncoded: false))
        if !hasRoot {
            guard let image = configuration.initialImage else {
                throw LinuxError.ENOENT
            }
            try await image.install(to: configuration.root, progress: configuration.installationProgress)
        }

        let callbacks = RuntimeCallbacks(
            consoleOutput: configuration.initMode == .system ? configuration.consoleOutput : nil,
            haltHandler: configuration.haltHandler,
            panicHandler: configuration.panicHandler
        )
        let bootedHandle = try await blocking {
            try bootKernel(configuration, callbacks: callbacks)
        }

        let runtime = LinuxRuntime(configuration: configuration, handle: bootedHandle.value, callbacks: callbacks)
        let initCommand: (path: String, arguments: [String], environment: [String])?
        switch configuration.initMode {
        case .managed:
            initCommand = nil
        case .system:
            initCommand = ("/sbin/init", ["/sbin/init"], ["PATH=\(Self.systemPath)", "TERM=linux"])
        }

        if configuration.autoSyncDNS {
            try? runtime.syncDNS()
            let monitor = NWPathMonitor()
            monitor.pathUpdateHandler = { [weak runtime] _ in
                try? runtime?.syncDNS()
            }
            monitor.start(queue: DispatchQueue(label: "SwiptermKit.PathMonitor"))
            let installed = runtime.stateCondition.withLock { () -> Bool in
                guard !runtime.isShutDown else { return false }
                runtime.pathMonitor = monitor
                return true
            }
            if !installed {
                monitor.cancel()
            }
        }
        do {
            try Task.checkCancellation()
            try await prepare(runtime)
            try Task.checkCancellation()
            try await runtime.withActiveHandleBlocking { handle in
                try Self.startInit(handle: handle, initCommand)
            }
        } catch {
            try? await blocking {
                runtime.shutdown()
            }
            throw error
        }
        return runtime
    }

    private static func startInit(
        handle: OpaquePointer,
        _ command: (path: String, arguments: [String], environment: [String])?
    ) throws(LinuxError) {
        let arguments = command?.arguments ?? []
        let code = withCStringArray(arguments) { argv in
            withCStringArray(command?.environment ?? []) { envp in
                withOptionalCString(command?.path) { path in
                    swipterm_start_init(handle, path, arguments.count, argv, envp)
                }
            }
        }
        try check(code)
    }

    private static func bootKernel(
        _ configuration: Configuration,
        callbacks: RuntimeCallbacks
    ) throws(LinuxError) -> UnsafeSendable<OpaquePointer> {
        let callbackTable = callbacks.makeTable(
            includeConsoleOutput: callbacks.consoleOutput != nil,
            includePanic: configuration.panicHandler != nil
        )
        var handle: OpaquePointer?
        let code = configuration.root.path(percentEncoded: false).withCString { root in
            withOptionalCString(configuration.hostname) { hostname in
                withOptionalCString(configuration.hostCommandsDirectory) { hostCommandsDirectory in
                    withOptionalCString(configuration.extraKernelArguments) { extraCmdline in
                        configuration.kernelImage.withCBlob { kernelBlob in
                            configuration.initramfs.withCBlob { initramfsBlob in
                                let modulesSource = configuration.modulesInitramfs ?? .embedded
                                return modulesSource.withCBlob { modulesBlob in
                                    func callBoot(_ rootImage: swipterm_root_image) -> Int32 {
                                        var options = swipterm_options(
                                            root_path: root,
                                            root_image: rootImage,
                                            hostname: hostname,
                                            host_commands_directory: hostCommandsDirectory,
                                            ram_size: 0,
                                            cpu_count: 0,
                                            callbacks: callbackTable,
                                            kernel_image: kernelBlob,
                                            initramfs: initramfsBlob,
                                            modules_initramfs: modulesBlob,
                                            extra_cmdline: extraCmdline
                                        )
                                        return swipterm_boot(&options, &handle)
                                    }
                                    if let image = configuration.initialImage {
                                        return image.withCRootImage(callBoot)
                                    }
                                    return callBoot(swipterm_root_image(
                                        kind: SWIPTERM_ROOT_IMAGE_NONE,
                                        path: nil,
                                        data: nil,
                                        size: 0
                                    ))
                                }
                            }
                        }
                    }
                }
            }
        }
        try check(code)
        return UnsafeSendable(value: handle!)
    }

    public func suspend() {
        try? withActiveHandle { swipterm_suspend($0) }
    }

    public func resume() {
        guard (try? withActiveHandle { swipterm_resume($0) }) != nil else { return }
        try? syncDNS()
    }

    public func syncDNS() throws(LinuxError) {
        try withActiveHandle { handle throws(LinuxError) in
            try check(swipterm_sync_dns(handle))
        }
    }

    /// Throws `LinuxError.ETIMEDOUT` after `timeout`, and `LinuxError.ESRCH` when the guest init has exited.
    public func waitReady(_ readiness: Readiness, timeout: Duration? = nil) async throws {
        let level = readiness == .login ? SWIPTERM_READY_LOGIN : SWIPTERM_READY_SYSTEM
        let clock = ContinuousClock()
        let deadline = timeout.map { clock.now.advanced(by: $0) }
        while true {
            try Task.checkCancellation()
            var interval = Self.readinessPollInterval
            if let deadline {
                let remaining = clock.now.duration(to: deadline)
                guard remaining > .zero else {
                    throw LinuxError.ETIMEDOUT
                }
                interval = min(interval, remaining)
            }
            let milliseconds = Int32(max(1, interval.components.seconds * 1000 + interval.components.attoseconds / 1_000_000_000_000_000))
            let code = try await withActiveHandleBlocking { handle in
                swipterm_wait_ready(handle, level, milliseconds)
            }
            if code == 0 {
                return
            }
            if code != -LinuxError.ETIMEDOUT.rawValue {
                try check(code)
            }
        }
    }

    /// Opens the guest's own login for `user` on `terminal`, as a preauthenticated local login.
    /// `user` names an account in the guest's `/etc/passwd`; a missing account throws `LinuxError.ENOENT`.
    /// The guest keeps `TERM`, `COLORTERM`, `LANG`, `LANGUAGE`, `LC_*`, `TZ` and `BROWSER` from `environment`.
    /// Until `waitReady(.login)` returns, the guest can refuse the login with `LinuxError.EAGAIN`.
    public func login(
        user: String = "root",
        on terminal: LinuxTerminal,
        environment: [String: String] = [:]
    ) async throws -> LinuxProcess {
        try await LinuxProcess(runtime: self, loginAs: user, on: terminal, environment: environment)
    }

    /// Replaces the writable layer of the stopped root at `root` with an empty one, keeping its root image.
    /// Throws `LinuxError.EBUSY` while a runtime uses `root`.
    public static func resetUpper(at root: URL) throws(LinuxError) {
        try check(root.withUnsafeFileSystemRepresentation { swipterm_reset_upper($0) })
    }

    public func makeTerminal(size: LinuxTerminal.Size) throws(LinuxError) -> LinuxTerminal {
        try LinuxTerminal(runtime: self, size: size)
    }

    public func spawn(_ configuration: LinuxProcess.Configuration) async throws -> LinuxProcess {
        try await LinuxProcess(runtime: self, configuration, environment: configuration.environment ?? defaultEnvironment)
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

    private func beginOperation() throws(LinuxError) {
        stateCondition.lock()
        if isShutDown {
            stateCondition.unlock()
            throw .EIO
        }
        activeOperations += 1
        stateCondition.unlock()
    }

    private func endOperation() {
        stateCondition.withLock {
            activeOperations -= 1
            if activeOperations == 0 {
                stateCondition.broadcast()
            }
        }
    }

    func withActiveHandle<Result>(_ body: (OpaquePointer) throws(LinuxError) -> Result) throws(LinuxError) -> Result {
        try beginOperation()
        defer { endOperation() }
        return try body(handle)
    }

    func withActiveHandleBlocking<Result: Sendable>(
        _ body: @escaping @Sendable (OpaquePointer) throws -> Result
    ) async throws -> Result {
        try beginOperation()
        defer { endOperation() }
        let runtimeHandle = UnsafeSendable(value: handle)
        return try await blocking {
            try body(runtimeHandle.value)
        }
    }

    public func mount(_ hostDirectory: URL, at guestPath: String, readOnly: Bool = false) async throws {
        let hostPath = hostDirectory.path(percentEncoded: false)
        try await withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_mount(runtimeHandle, hostPath, guestPath, readOnly))
        }
    }

    public func unmount(at guestPath: String) async throws {
        try await withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_unmount(runtimeHandle, guestPath))
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
        try beginOperation()
        let command = Unmanaged.passRetained(CommandBox(name: name, handler: handler))
        let context = UnsafeSendable(value: command.toOpaque())
        let runtimeHandle = UnsafeSendable(value: handle)
        do {
            try await blocking {
                try check(swipterm_register_command(runtimeHandle.value, name, context.value))
            }
        } catch {
            endOperation()
            command.release()
            throw error
        }
        stateCondition.withLock {
            registeredCommands.append(command)
            activeOperations -= 1
            if activeOperations == 0 {
                stateCondition.broadcast()
            }
        }
    }

    public func registerDevice(at path: String, handler: some LinuxCharacterDevice) async throws {
        try beginOperation()
        let device = Unmanaged.passRetained(DeviceBox(device: handler))
        let context = UnsafeSendable(value: device.toOpaque())
        let runtimeHandle = UnsafeSendable(value: handle)
        do {
            try await blocking {
                try check(swipterm_register_device(runtimeHandle.value, path, context.value))
            }
        } catch {
            endOperation()
            device.release()
            throw error
        }
        stateCondition.withLock {
            registeredDevices.append(device)
            activeOperations -= 1
            if activeOperations == 0 {
                stateCondition.broadcast()
            }
        }
    }
}
