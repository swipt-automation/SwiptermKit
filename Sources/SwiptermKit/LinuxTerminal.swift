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

    let handle: OpaquePointer
    private let runtime: LinuxRuntime
    private let notifyDescriptor: Int32
    private let queue = DispatchQueue(label: "SwiptermKit.LinuxTerminal")
    private let inputQueue = DispatchQueue(label: "SwiptermKit.LinuxTerminal.Input")
    private let events: DispatchEventSource
    private var requests: [IORequest<Data?>] = []
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

    public func read() async -> Data? {
        let request = IORequest<Data?>()
        return try? await request.submit(on: queue) {
            self.requests.append(request)
            self.serve()
        } withdraw: {
            self.requests.removeAll { $0 === request }
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
        requests.removeFirst().complete(.success(data))
    }

    private func serve() {
        var byte: UInt8 = 0
        while Darwin.read(notifyDescriptor, &byte, 1) > 0 {}
        while !requests.isEmpty {
            if ended {
                complete(nil)
                continue
            }
            var data = Data(count: 65536)
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

public extension LinuxTerminal {
    struct Output: AsyncSequence, Sendable {
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
