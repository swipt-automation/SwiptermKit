import CSwiptermKit
import Foundation
import LibSwipterm
import Network

public final class LinuxRuntime: @unchecked Sendable {
    public struct Configuration: Sendable {
        public enum InitMode: Hashable, Sendable {
            case system
            case managed
        }

        public enum Kernel: Hashable, Sendable {
            case installed
            case release(String)
        }

        let root: URL
        let initMode: InitMode
        let kernel: Kernel
        let hostname: String
        let gpu: GPUConfiguration?
        let consoleOutput: (@Sendable (Data) -> Void)?
        let haltHandler: (@Sendable (HaltReason) -> Void)?
        let clipboard: (any LinuxClipboard)?

        public init(
            root: URL,
            initMode: InitMode = .system,
            kernel: Kernel = .installed,
            hostname: String,
            gpu: GPUConfiguration? = nil,
            consoleOutput: (@Sendable (Data) -> Void)? = nil,
            haltHandler: (@Sendable (HaltReason) -> Void)? = nil,
            clipboard: (any LinuxClipboard)? = nil
        ) {
            self.root = root
            self.initMode = initMode
            self.kernel = kernel
            self.hostname = hostname
            self.gpu = gpu
            self.consoleOutput = consoleOutput
            self.haltHandler = haltHandler
            self.clipboard = clipboard
        }
    }

    public struct KernelUnavailableError: LocalizedError, Sendable {
        let reason: String

        public var errorDescription: String? {
            reason.prefix(1).uppercased() + reason.dropFirst() + "."
        }
    }

    public struct ProcessEnergySample: Sendable, Equatable {
        public let name: String
        public let energyNanojoules: UInt64
        public let cpuMicroseconds: UInt64

        public init(name: String, energyNanojoules: UInt64, cpuMicroseconds: UInt64) {
            self.name = name
            self.energyNanojoules = energyNanojoules
            self.cpuMicroseconds = cpuMicroseconds
        }
    }

    public enum HaltReason: Hashable, Sendable {
        case reboot
        case powerOff
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

    public static var isJITPrepared: Bool {
        swipterm_jit_is_prepared()
    }

    public static var isJITEnabled: Bool {
        get { swipterm_jit_is_enabled() }
        set { swipterm_jit_set_enabled(newValue) }
    }

    /// On iOS the process must be attached to a debugger that answers `brk #0xf00d` (`x16 = 1` prepares the region,
    /// `x16 = 0` detaches), and the debugger is asked to detach before this returns.
    public static func prepareJIT() throws {
        var message: UnsafeMutablePointer<CChar>?
        guard swipterm_jit_prepare(&message) else {
            throw MessageError(consuming: message)
        }
    }

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

    private init(handle: OpaquePointer, network: LibswiptermNetwork, callbacks: RuntimeCallbacks) {
        self.handle = handle
        self.network = network
        self.callbacks = callbacks
        let monitor = NWPathMonitor()
        monitor.pathUpdateHandler = { [weak self] path in
            guard let self else { return }
            let previous = networkPath
            networkPath = path
            guard let previous,
                  path.status != previous.status || path.availableInterfaces.first != previous.availableInterfaces.first
            else { return }
            try? withActiveHandle { _ in
                self.network.resetNetwork()
            }
        }
        monitor.start(queue: DispatchQueue(label: "SwiptermKit.PathMonitor"))
        pathMonitor = monitor
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

    public static func boot(
        _ configuration: Configuration,
        prepare: @Sendable (LinuxRuntime) async throws -> Void
    ) async throws -> LinuxRuntime {
        guard LinuxRootImage.isInstalled(at: configuration.root) else {
            throw LinuxError.ENOENT
        }

        let callbacks = RuntimeCallbacks(
            consoleOutput: configuration.initMode == .system ? configuration.consoleOutput : nil,
            haltHandler: configuration.haltHandler,
            clipboard: configuration.clipboard
        )
        var networkError: NSError?
        guard let network = LibswiptermCreateNetwork(&networkError) else {
            throw networkError!
        }
        let networkHandle = UnsafeSendable(value: network)
        let handle: UnsafeSendable<OpaquePointer>
        do {
            handle = try await blocking {
                try bootKernel(configuration, callbacks: callbacks, network: networkHandle.value)
            }
        } catch {
            try? network.close()
            throw error
        }

        let runtime = LinuxRuntime(handle: handle.value, network: network, callbacks: callbacks)
        let initCommand: (path: String, arguments: [String], environment: [String])?
        switch configuration.initMode {
        case .managed:
            initCommand = nil
        case .system:
            initCommand = ("/sbin/init", ["/sbin/init"], ["PATH=\(Self.systemPath)", "TERM=linux"])
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
    ) throws -> UnsafeSendable<OpaquePointer> {
        guard let initramfs = Bundle.module.url(forResource: "initramfs.cpio", withExtension: "gz") else {
            throw LinuxError.ENOENT
        }
        let selection: swipterm_kernel_selection
        let release: String?
        switch configuration.kernel {
        case .installed:
            selection = SWIPTERM_KERNEL_INSTALLED
            release = nil
        case let .release(name):
            selection = SWIPTERM_KERNEL_RELEASE
            release = name
        }
        var handle: OpaquePointer?
        var message: UnsafeMutablePointer<CChar>?
        let code = configuration.root.withUnsafeFileSystemRepresentation { root in
            initramfs.withUnsafeFileSystemRepresentation { initramfsPath in
                configuration.hostname.withCString { hostname in
                    withOptionalCString(release) { release in
                        var options = swipterm_options(
                            root_path: root,
                            hostname: hostname,
                            callbacks: callbacks.makeTable(),
                            initramfs_path: initramfsPath,
                            kernel: selection,
                            kernel_release: release,
                            network: OpaquePointer(bitPattern: Int(network.handle())),
                            gpu: configuration.gpu?.cValue ?? swipterm_gpu_options()
                        )
                        return swipterm_boot(&options, &handle, &message)
                    }
                }
            }
        }
        if let message {
            defer { free(message) }
            throw KernelUnavailableError(reason: String(cString: message))
        }
        try check(code)
        return UnsafeSendable(value: handle!)
    }

    public func suspend() {
        try? withActiveHandle { swipterm_suspend($0) }
    }

    public func resume() {
        try? withActiveHandle { swipterm_resume($0) }
    }

    public func waitReady(timeout: Duration) async throws {
        try await withThrowingTaskGroup(of: Void.self) { group in
            group.addTask {
                try await self.callbacks.readiness.wait()
            }
            group.addTask {
                try await Task.sleep(for: timeout)
                throw LinuxError.ETIMEDOUT
            }
            _ = try await group.next()
            group.cancelAll()
        }
    }

    public func spawn(_ configuration: LinuxProcess.Configuration) async throws -> LinuxProcess {
        try await LinuxProcess(runtime: self, disk: 0, configuration, hostname: "", shell: false, invocation: nil)
    }

    public func login(
        user: String,
        on terminal: LinuxTerminal,
        environment: [String: String]
    ) async throws -> LinuxProcess {
        try await LinuxProcess(runtime: self, loginAs: user, on: terminal, environment: environment)
    }

    public func makeTerminal(size: LinuxTerminal.Size) throws(LinuxError) -> LinuxTerminal {
        try LinuxTerminal(runtime: self, size: size)
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

    public func mount(_ hostDirectory: URL, at guestPath: String, readOnly: Bool) async throws {
        let hostPath = hostDirectory.path(percentEncoded: false)
        try await withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_mount(runtimeHandle, hostPath, guestPath, readOnly))
        }
    }

    public func attachDisk(root: URL, readOnly: Bool) async throws -> LinuxDisk {
        try await LinuxDisk(runtime: self, root: root, readOnly: readOnly)
    }

    public func registerCommand(
        named name: String,
        handler: @escaping @Sendable (HostCommandInvocation) async throws -> Int32
    ) async throws {
        try await register(CommandBox(name: name, handler: handler), in: \.registeredCommands) { runtimeHandle, context in
            swipterm_register_command(runtimeHandle, name, context)
        }
    }

    public func registerDevice(at path: String, handler: some LinuxCharacterDevice) async throws {
        try await register(DeviceBox(device: handler), in: \.registeredDevices) { runtimeHandle, context in
            swipterm_register_device(runtimeHandle, path, context)
        }
    }

    private func register<Box: AnyObject>(
        _ box: Box,
        in registry: ReferenceWritableKeyPath<LinuxRuntime, [Unmanaged<Box>]>,
        _ body: @escaping @Sendable (OpaquePointer, UnsafeMutableRawPointer) -> Int32
    ) async throws {
        try beginOperation()
        defer { endOperation() }
        let registered = Unmanaged.passRetained(box)
        let context = UnsafeSendable(value: registered.toOpaque())
        let runtimeHandle = UnsafeSendable(value: handle)
        do {
            try await blocking {
                try check(body(runtimeHandle.value, context.value))
            }
        } catch {
            registered.release()
            throw error
        }
        stateCondition.withLock {
            self[keyPath: registry].append(registered)
        }
    }

    public func setClipboardOffers(_ mimes: [String]) throws(LinuxError) {
        let code = try withActiveHandle { runtimeHandle in
            withCStringArray(mimes) { array in
                swipterm_set_clipboard_offers(runtimeHandle, mimes.count, array)
            }
        }
        try check(code)
    }

    private final class UsageCollectorBox {
        var samples: [ProcessEnergySample] = []
    }

    public func collectProcessEnergy(minimumCPUNanoseconds: UInt64 = 0) async throws -> [ProcessEnergySample] {
        try await withActiveHandleBlocking { runtimeHandle in
            let box = UsageCollectorBox()
            let code = withExtendedLifetime(box) {
                swipterm_collect_usage(runtimeHandle, minimumCPUNanoseconds, Unmanaged.passUnretained(box).toOpaque()) { context, count, names, energy, cpu in
                    guard let context, let names, let energy, let cpu else { return }
                    let collector = Unmanaged<UsageCollectorBox>.fromOpaque(context).takeUnretainedValue()
                    collector.samples.reserveCapacity(count)
                    for i in 0 ..< count {
                        guard let cName = names[i] else { continue }
                        collector.samples.append(ProcessEnergySample(
                            name: String(cString: cName),
                            energyNanojoules: energy[i],
                            cpuMicroseconds: cpu[i]
                        ))
                    }
                }
            }
            try check(code)
            return box.samples
        }
    }
}
