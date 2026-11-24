import CSwiptermKit
import Foundation
import LibSwipterm
import Network

public final class LinuxRuntime: @unchecked Sendable {
    public struct Configuration: Sendable {
        public enum InitMode: Hashable, Sendable {
            /// The root's `/sbin/init` runs as PID 1 of a system container.
            case system
            /// No guest init runs; the runtime supervises the processes it starts.
            case managed
        }

        /// Booting throws `LinuxRuntime.KernelUnavailableError` when no kernel matches.
        public enum Kernel: Hashable, Sendable {
            /// The newest kernel installed in the root that can boot this machine.
            case installed
            /// The installed kernel with this release.
            case release(String)
        }

        public var root: URL
        public var initMode: InitMode
        public var kernel: Kernel
        public var hostname: String?
        public var hostCommandsDirectory: String?
        public var memorySize: UInt64?
        public var cpuCount: Int?
        public var extraKernelArguments: String?
        public var consoleOutput: (@Sendable (Data) -> Void)?
        public var panicHandler: (@Sendable (String) -> Void)?
        public var haltHandler: (@Sendable (HaltReason) -> Void)?

        public init(
            root: URL,
            initMode: InitMode = .system,
            kernel: Kernel = .installed,
            hostname: String? = nil,
            hostCommandsDirectory: String? = "/run/host-bin",
            memorySize: UInt64? = nil,
            cpuCount: Int? = nil,
            extraKernelArguments: String? = nil,
            consoleOutput: (@Sendable (Data) -> Void)? = nil,
            panicHandler: (@Sendable (String) -> Void)? = nil,
            haltHandler: (@Sendable (HaltReason) -> Void)? = nil
        ) {
            self.root = root
            self.initMode = initMode
            self.kernel = kernel
            self.hostname = hostname
            self.hostCommandsDirectory = hostCommandsDirectory
            self.memorySize = memorySize
            self.cpuCount = cpuCount
            self.extraKernelArguments = extraKernelArguments
            self.consoleOutput = consoleOutput
            self.panicHandler = panicHandler
            self.haltHandler = haltHandler
        }
    }

    public struct BootedKernel: Hashable, Sendable {
        public let release: String
        /// Why the newer installed kernels did not boot; `nil` when the newest did.
        public let fallbackReason: String?

        init(_ info: swipterm_boot_info) {
            var info = info
            release = withUnsafeBytes(of: &info.release) { String(cString: $0.bindMemory(to: CChar.self).baseAddress!) }
            let reason = withUnsafeBytes(of: &info.fallback_reason) {
                String(cString: $0.bindMemory(to: CChar.self).baseAddress!)
            }
            fallbackReason = reason.isEmpty ? nil : reason
        }
    }

    public struct KernelUnavailableError: LocalizedError, Sendable {
        /// Why each installed kernel that `Configuration.kernel` selects cannot boot this machine; empty when the root has none.
        public let reasons: [String]

        public var errorDescription: String? {
            guard !reasons.isEmpty else {
                return "No kernel is installed in the root."
            }
            return "No installed kernel can boot this machine: " + reasons.joined(separator: "; ")
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

    public struct JITError: LocalizedError, Sendable {
        public let message: String

        public var errorDescription: String? {
            message
        }
    }

    /// While this and `isJITEnabled` are both `true`, guest code runs through the JIT, otherwise through the interpreter.
    /// Running machines follow changes to either at the next vCPU safe point without restarting.
    public static var isJITPrepared: Bool {
        swipterm_jit_is_prepared()
    }

    public static var isJITEnabled: Bool {
        get { swipterm_jit_is_enabled() }
        set { swipterm_jit_set_enabled(newValue) }
    }

    /// Prepares the process-wide executable region once; later calls succeed immediately once prepared.
    /// On iOS the process must be attached to a debugger that answers `brk #0xf00d` (`x16 = 1` prepares the region,
    /// `x16 = 0` detaches), and the debugger is asked to detach before this returns.
    public static func prepareJIT() throws {
        var message: UnsafeMutablePointer<CChar>?
        guard swipterm_jit_prepare(&message) else {
            defer { free(message) }
            throw JITError(message: message.map { String(cString: $0) } ?? "Failed to prepare executable memory.")
        }
    }

    public let defaultEnvironment: [String: String]
    public let bootedKernel: BootedKernel
    public var fileSystem: LinuxFileSystem {
        LinuxFileSystem(runtime: self)
    }

    let handle: OpaquePointer
    private let network: LibswiptermNetwork
    private let callbacks: RuntimeCallbacks
    private let stateCondition = NSCondition()
    private var pathMonitor: NWPathMonitor?
    private var networkPath: NWPath?
    private var registeredCommands: [Unmanaged<CommandBox>] = []
    private var registeredDevices: [Unmanaged<DeviceBox>] = []
    private var activeOperations = 0
    private var isShutDown = false
    private var isShutdownComplete = false

    private static let systemPath = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
    private static let readinessPollInterval = Duration.milliseconds(250)

    private init(
        configuration: Configuration,
        handle: OpaquePointer,
        network: LibswiptermNetwork,
        callbacks: RuntimeCallbacks,
        bootedKernel: BootedKernel
    ) {
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
        self.network = network
        self.callbacks = callbacks
        self.bootedKernel = bootedKernel
    }

    deinit {
        guard !isShutDown else {
            return
        }
        let teardown = UnsafeSendable(value: (
            handle: handle,
            network: network,
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
            try? state.network.close()
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
        try? network.close()
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
        guard LinuxRootImage.isInstalled(at: configuration.root) else {
            throw LinuxError.ENOENT
        }

        let callbacks = RuntimeCallbacks(
            consoleOutput: configuration.initMode == .system ? configuration.consoleOutput : nil,
            haltHandler: configuration.haltHandler,
            panicHandler: configuration.panicHandler
        )
        var networkError: NSError?
        guard let network = LibswiptermCreateNetwork(&networkError) else {
            throw networkError!
        }
        let networkHandle = UnsafeSendable(value: network)
        let booted: (handle: UnsafeSendable<OpaquePointer>, kernel: BootedKernel)
        do {
            booted = try await blocking {
                try bootKernel(configuration, callbacks: callbacks, network: networkHandle.value)
            }
        } catch {
            try? network.close()
            throw error
        }

        let runtime = LinuxRuntime(
            configuration: configuration,
            handle: booted.handle.value,
            network: network,
            callbacks: callbacks,
            bootedKernel: booted.kernel
        )
        let initCommand: (path: String, arguments: [String], environment: [String])?
        switch configuration.initMode {
        case .managed:
            initCommand = nil
        case .system:
            initCommand = ("/sbin/init", ["/sbin/init"], ["PATH=\(Self.systemPath)", "TERM=linux"])
        }

        let monitor = NWPathMonitor()
        monitor.pathUpdateHandler = { [weak runtime] path in
            guard let runtime else { return }
            let previous = runtime.networkPath
            runtime.networkPath = path
            guard let previous,
                  path.status != previous.status || path.availableInterfaces.first != previous.availableInterfaces.first
            else { return }
            try? runtime.withActiveHandle { _ in
                runtime.network.resetNetwork()
            }
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
        callbacks: RuntimeCallbacks,
        network: LibswiptermNetwork
    ) throws -> (handle: UnsafeSendable<OpaquePointer>, kernel: BootedKernel) {
        guard let initramfs = Bundle.module.url(forResource: "initramfs.cpio", withExtension: "gz") else {
            throw LinuxError.ENOENT
        }
        let callbackTable = callbacks.makeTable(
            includeConsoleOutput: callbacks.consoleOutput != nil,
            includePanic: configuration.panicHandler != nil
        )
        let selection: swipterm_kernel_selection
        let release: String?
        switch configuration.kernel {
        case .installed:
            selection = SWIPTERM_KERNEL_INSTALLED
            release = nil
        case .release(let name):
            selection = SWIPTERM_KERNEL_RELEASE
            release = name
        }
        var handle: OpaquePointer?
        var message: UnsafeMutablePointer<CChar>?
        let code = configuration.root.withUnsafeFileSystemRepresentation { root in
            initramfs.withUnsafeFileSystemRepresentation { initramfsPath in
                withOptionalCString(configuration.hostname) { hostname in
                    withOptionalCString(configuration.hostCommandsDirectory) { hostCommandsDirectory in
                        withOptionalCString(configuration.extraKernelArguments) { extraCmdline in
                            withOptionalCString(release) { release in
                                var options = swipterm_options(
                                    root_path: root,
                                    hostname: hostname,
                                    host_commands_directory: hostCommandsDirectory,
                                    ram_size: Int(truncatingIfNeeded: configuration.memorySize ?? 0),
                                    cpu_count: UInt32(truncatingIfNeeded: configuration.cpuCount ?? 0),
                                    callbacks: callbackTable,
                                    initramfs_path: initramfsPath,
                                    kernel: selection,
                                    kernel_release: release,
                                    extra_cmdline: extraCmdline,
                                    network: OpaquePointer(bitPattern: Int(network.handle()))
                                )
                                return swipterm_boot(&options, &handle, &message)
                            }
                        }
                    }
                }
            }
        }
        if let message {
            defer { free(message) }
            let reasons = String(cString: message)
            throw KernelUnavailableError(reasons: reasons.isEmpty ? [] : reasons.components(separatedBy: "\n"))
        }
        try check(code)
        var info = swipterm_boot_info()
        swipterm_get_boot_info(handle, &info)
        return (UnsafeSendable(value: handle!), BootedKernel(info))
    }

    public func suspend() {
        try? withActiveHandle { swipterm_suspend($0) }
    }

    public func resume() {
        try? withActiveHandle { swipterm_resume($0) }
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
