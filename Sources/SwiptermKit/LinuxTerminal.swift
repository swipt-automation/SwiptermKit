import CSwiptermKit
import Foundation

public final class LinuxTerminal: @unchecked Sendable {
    public struct Size: Hashable, Sendable {
        public var columns: UInt16
        public var rows: UInt16

        public init(columns: UInt16, rows: UInt16) {
            self.columns = columns
            self.rows = rows
        }
    }

    private final class Request: @unchecked Sendable {
        let maxLength: Int
        var continuation: CheckedContinuation<Data?, Never>?
        var cancelled = false

        init(maxLength: Int) {
            self.maxLength = maxLength
        }
    }

    let handle: OpaquePointer
    private let runtime: LinuxRuntime
    private let notifyDescriptor: Int32
    private let queue = DispatchQueue(label: "SwiptermKit.LinuxTerminal")
    private let inputQueue = DispatchQueue(label: "SwiptermKit.LinuxTerminal.Input")
    private let events: DispatchEventSource
    private var requests: [Request] = []
    private var ended = false

    init(runtime: LinuxRuntime, size: Size) throws(LinuxError) {
        let handle = try runtime.withActiveHandle { runtimeHandle throws(LinuxError) -> OpaquePointer in
            var error: Int32 = 0
            guard let handle = swipterm_terminal_create(runtimeHandle, size.columns, size.rows, &error) else {
                throw LinuxError(code: error)
            }
            return handle
        }
        self.runtime = runtime
        self.handle = handle
        notifyDescriptor = swipterm_terminal_notify_fd(handle)
        let source = DispatchSource.makeReadSource(fileDescriptor: notifyDescriptor, queue: queue)
        events = DispatchEventSource(source)
        source.setEventHandler { [weak self] in
            self?.serve()
        }
        let releasedHandle = UnsafeSendable(value: handle)
        source.setCancelHandler {
            swipterm_terminal_release(releasedHandle.value)
        }
    }

    deinit {
        swipterm_terminal_close(handle)
        events.cancel()
    }

    public func read(maxLength: Int = 65536) async -> Data? {
        precondition(maxLength > 0)
        let request = Request(maxLength: maxLength)
        return await withTaskCancellationHandler {
            await withCheckedContinuation { continuation in
                queue.async {
                    if request.cancelled {
                        continuation.resume(returning: nil)
                        return
                    }
                    request.continuation = continuation
                    self.requests.append(request)
                    self.serve()
                }
            }
        } onCancel: {
            queue.async {
                request.cancelled = true
                if let continuation = request.continuation {
                    request.continuation = nil
                    self.requests.removeAll { $0 === request }
                    continuation.resume(returning: nil)
                }
            }
        }
    }

    public var output: Output {
        Output(terminal: self)
    }

    public func write(_ data: Data) async {
        await withCheckedContinuation { continuation in
            inputQueue.async {
                data.withUnsafeBytes { buffer in
                    _ = swipterm_terminal_write(self.handle, buffer.baseAddress, buffer.count)
                }
                continuation.resume()
            }
        }
    }

    public func resize(_ size: Size) {
        swipterm_terminal_resize(handle, size.columns, size.rows)
    }

    public func close() {
        swipterm_terminal_close(handle)
    }

    private func complete(_ data: Data?) {
        let request = requests.removeFirst()
        request.continuation?.resume(returning: data)
        request.continuation = nil
    }

    private func serve() {
        var byte: UInt8 = 0
        while Darwin.read(notifyDescriptor, &byte, 1) > 0 {}
        while let request = requests.first {
            if ended {
                complete(nil)
                continue
            }
            var data = Data(count: request.maxLength)
            let count = data.withUnsafeMutableBytes { swipterm_terminal_read(handle, $0.baseAddress, $0.count) }
            if count > 0 {
                data.count = count
                complete(data)
            } else if count == 0 {
                ended = true
            } else {
                events.activate()
                return
            }
        }
        events.deactivate()
    }
}

extension LinuxTerminal {
    public struct Output: AsyncSequence, Sendable {
        public typealias Element = Data

        let terminal: LinuxTerminal

        public struct AsyncIterator: AsyncIteratorProtocol {
            let terminal: LinuxTerminal

            public mutating func next() async -> Data? {
                await terminal.read()
            }
        }

        public func makeAsyncIterator() -> AsyncIterator {
            AsyncIterator(terminal: terminal)
        }
    }
}
