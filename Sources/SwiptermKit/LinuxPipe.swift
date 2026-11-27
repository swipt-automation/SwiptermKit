import Foundation

private func posixError() -> POSIXError {
    POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO)
}

private func setNonBlocking(_ fileDescriptor: Int32) {
    _ = fcntl(fileDescriptor, F_SETFL, fcntl(fileDescriptor, F_GETFL) | O_NONBLOCK)
}

final class LinuxPipeReader: @unchecked Sendable {
    private let fileDescriptor: Int32
    private let queue = DispatchQueue(label: "SwiptermKit.LinuxPipeReader")
    private let events: DispatchEventSource
    private var requests: [IORequest<Data?>] = []
    private var ended = false
    private var closed = false

    init(fileDescriptor: Int32) {
        self.fileDescriptor = fileDescriptor
        setNonBlocking(fileDescriptor)
        let source = DispatchSource.makeReadSource(fileDescriptor: fileDescriptor, queue: queue)
        events = DispatchEventSource(source)
        source.setEventHandler { [weak self] in
            self?.serve()
        }
        source.setCancelHandler {
            Darwin.close(fileDescriptor)
        }
    }

    deinit {
        if !closed {
            events.cancel()
        }
    }

    func read() async throws -> Data? {
        let request = IORequest<Data?>()
        return try await request.submit(on: queue) {
            self.requests.append(request)
            self.serve()
        } withdraw: {
            self.requests.removeAll { $0 === request }
        }
    }

    func close() {
        queue.async {
            guard !self.closed else { return }
            self.closed = true
            self.events.cancel()
            self.serve()
        }
    }

    private func complete(_ result: Result<Data?, any Error>) {
        requests.removeFirst().complete(result)
    }

    private func serve() {
        while !requests.isEmpty {
            if ended || closed {
                complete(.success(nil))
                continue
            }
            var data = Data(count: 65536)
            let count = data.withUnsafeMutableBytes { Darwin.read(fileDescriptor, $0.baseAddress, $0.count) }
            if count > 0 {
                data.count = count
                complete(.success(data))
            } else if count == 0 {
                ended = true
            } else if errno == EAGAIN {
                events.activate()
                return
            } else if errno != EINTR {
                complete(.failure(posixError()))
            }
        }
        if !closed {
            events.deactivate()
        }
    }
}

public final class LinuxPipeWriter: @unchecked Sendable {
    private let fileDescriptor: Int32
    private let queue = DispatchQueue(label: "SwiptermKit.LinuxPipeWriter")
    private let events: DispatchEventSource
    private var requests: [(data: Data, request: IORequest<Void>)] = []
    private var written = 0
    private var closed = false

    init(fileDescriptor: Int32) {
        self.fileDescriptor = fileDescriptor
        setNonBlocking(fileDescriptor)
        let source = DispatchSource.makeWriteSource(fileDescriptor: fileDescriptor, queue: queue)
        events = DispatchEventSource(source)
        source.setEventHandler { [weak self] in
            self?.serve()
        }
        source.setCancelHandler {
            Darwin.close(fileDescriptor)
        }
    }

    deinit {
        if !closed {
            events.cancel()
        }
    }

    public func write(_ data: Data) async throws {
        let request = IORequest<Void>()
        try await request.submit(on: queue) {
            self.requests.append((data, request))
            self.serve()
        } withdraw: {
            if self.requests.first?.request === request {
                self.written = 0
            }
            self.requests.removeAll { $0.request === request }
        }
    }

    func close() {
        queue.async {
            guard !self.closed else { return }
            self.closed = true
            self.events.cancel()
            self.serve()
        }
    }

    private func complete(_ result: Result<Void, any Error>) {
        written = 0
        requests.removeFirst().request.complete(result)
    }

    private func serve() {
        while let data = requests.first?.data {
            if closed {
                complete(.failure(POSIXError(.EBADF)))
                continue
            }
            if written == data.count {
                complete(.success(()))
                continue
            }
            let count = data.withUnsafeBytes {
                Darwin.write(fileDescriptor, $0.baseAddress! + written, $0.count - written)
            }
            if count >= 0 {
                written += count
            } else if errno == EAGAIN {
                events.activate()
                return
            } else if errno != EINTR {
                complete(.failure(posixError()))
            }
        }
        if !closed {
            events.deactivate()
        }
    }
}
