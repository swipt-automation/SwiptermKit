import CSwiptermKit
import Foundation

public final class LinuxTerminal: @unchecked Sendable {
    public enum Termination: Sendable, Equatable {
        case exited(Int32)
        case signaled(Int32)

        init(status: Int32) {
            let signal = status & 0x7F
            self = signal != 0 ? .signaled(signal) : .exited((status >> 8) & 0xFF)
        }
    }

    private final class Handlers {
        let output: @Sendable (Data) -> Void
        let termination: @Sendable (Termination) -> Void

        init(output: @escaping @Sendable (Data) -> Void, termination: @escaping @Sendable (Termination) -> Void) {
            self.output = output
            self.termination = termination
        }
    }

    private let handle: OpaquePointer
    private let handlers: Unmanaged<Handlers>
    private let queue = DispatchQueue(label: "SwiptermKit.LinuxTerminal")
    private let lock = NSLock()
    private var closed = false

    public init(
        arguments: [String],
        environment: [String],
        columns: UInt16,
        rows: UInt16,
        output: @escaping @Sendable (Data) -> Void,
        termination: @escaping @Sendable (Termination) -> Void
    ) throws {
        let handlers = Unmanaged.passRetained(Handlers(output: output, termination: termination))
        var error: Int32 = 0
        let handle = withCStringArray(arguments) { argv in
            withCStringArray(environment) { envp in
                swipterm_terminal_open(argv, envp, columns, rows, swipterm_terminal_handlers(
                    context: handlers.toOpaque(),
                    output: { context, data, length in
                        Unmanaged<Handlers>.fromOpaque(context!).takeUnretainedValue()
                            .output(Data(bytes: data!, count: length))
                    },
                    exit: { context, status in
                        Unmanaged<Handlers>.fromOpaque(context!).takeUnretainedValue()
                            .termination(Termination(status: status))
                    }
                ), &error)
            }
        }
        guard let handle else {
            handlers.release()
            throw LinuxError(code: error)
        }
        self.handle = handle
        self.handlers = handlers
    }

    private var isOpen: Bool {
        lock.withLock { !closed }
    }

    public func sendInput(_ data: Data) {
        queue.async { [self] in
            guard isOpen else { return }
            data.withUnsafeBytes { buffer in
                swipterm_terminal_send_input(handle, buffer.baseAddress, buffer.count)
            }
        }
    }

    public func resize(columns: UInt16, rows: UInt16) {
        queue.async { [self] in
            guard isOpen else { return }
            swipterm_terminal_resize(handle, columns, rows)
        }
    }

    public func setOutputSuspended(_ suspended: Bool) {
        lock.withLock {
            guard !closed else { return }
            swipterm_terminal_set_output_suspended(handle, suspended)
        }
    }

    public func close() {
        let wasClosed = lock.withLock {
            defer { closed = true }
            return closed
        }
        guard !wasClosed else { return }
        queue.async { [self] in
            swipterm_terminal_close(handle)
            handlers.release()
        }
    }
}
