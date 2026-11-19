import Foundation

private func posixError() -> POSIXError {
    POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO)
}

private func setNonBlocking(_ fileDescriptor: Int32) {
    _ = fcntl(fileDescriptor, F_SETFL, fcntl(fileDescriptor, F_GETFL) | O_NONBLOCK)
}

public final class LinuxPipeReader: @unchecked Sendable {
    private final class Request: @unchecked Sendable {
        let maxLength: Int
        var continuation: CheckedContinuation<Data?, Error>?
        var cancelled = false

        init(maxLength: Int) {
            self.maxLength = maxLength
        }
    }

    private let fileDescriptor: Int32
    private let queue = DispatchQueue(label: "SwiptermKit.LinuxPipeReader")
    private let events: DispatchEventSource
    private var requests: [Request] = []
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

    public func read(maxLength: Int = 65536) async throws -> Data? {
        precondition(maxLength > 0)
        let request = Request(maxLength: maxLength)
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { continuation in
                queue.async {
                    if request.cancelled {
                        continuation.resume(throwing: CancellationError())
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
                    continuation.resume(throwing: CancellationError())
                }
            }
        }
    }

    public func readToEnd() async throws -> Data {
        var data = Data()
        while let chunk = try await read() {
            data.append(chunk)
        }
        return data
    }

    public func close() {
        queue.async {
            guard !self.closed else { return }
            self.closed = true
            self.events.cancel()
            self.serve()
        }
    }

    private func complete(_ result: Result<Data?, Error>) {
        let request = requests.removeFirst()
        request.continuation?.resume(with: result)
        request.continuation = nil
    }

    private func serve() {
        while let request = requests.first {
            if ended || closed {
                complete(.success(nil))
                continue
            }
            var data = Data(count: request.maxLength)
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

extension LinuxPipeReader: AsyncSequence {
    public typealias Element = Data

    public struct AsyncIterator: AsyncIteratorProtocol {
        let reader: LinuxPipeReader

        public mutating func next() async throws -> Data? {
            try await reader.read()
        }
    }

    public func makeAsyncIterator() -> AsyncIterator {
        AsyncIterator(reader: self)
    }
}

public final class LinuxPipeWriter: @unchecked Sendable {
    private final class Request: @unchecked Sendable {
        let data: Data
        var offset = 0
        var continuation: CheckedContinuation<Void, Error>?
        var cancelled = false

        init(data: Data) {
            self.data = data
        }
    }

    private let fileDescriptor: Int32
    private let queue = DispatchQueue(label: "SwiptermKit.LinuxPipeWriter")
    private let events: DispatchEventSource
    private var requests: [Request] = []
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
        let request = Request(data: data)
        try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
                queue.async {
                    if request.cancelled {
                        continuation.resume(throwing: CancellationError())
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
                    continuation.resume(throwing: CancellationError())
                }
            }
        }
    }

    public func close() {
        queue.async {
            guard !self.closed else { return }
            self.closed = true
            self.events.cancel()
            self.serve()
        }
    }

    private func complete(_ result: Result<Void, Error>) {
        let request = requests.removeFirst()
        request.continuation?.resume(with: result)
        request.continuation = nil
    }

    private func serve() {
        while let request = requests.first {
            if closed {
                complete(.failure(POSIXError(.EBADF)))
                continue
            }
            if request.offset == request.data.count {
                complete(.success(()))
                continue
            }
            let count = request.data.withUnsafeBytes {
                Darwin.write(fileDescriptor, $0.baseAddress! + request.offset, $0.count - request.offset)
            }
            if count >= 0 {
                request.offset += count
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
