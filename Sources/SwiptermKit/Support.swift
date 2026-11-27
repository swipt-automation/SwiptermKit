import Foundation

func blocking<Result: Sendable>(_ body: @escaping @Sendable () throws -> Result) async throws -> Result {
    try await withCheckedThrowingContinuation { continuation in
        DispatchQueue.global().async {
            continuation.resume(with: Swift.Result { try body() })
        }
    }
}

func withCStringArray<Result>(
    _ strings: [String],
    _ body: (UnsafePointer<UnsafePointer<CChar>?>) throws -> Result
) rethrows -> Result {
    let pointers = strings.map { UnsafePointer(strdup($0)) } + [nil]
    defer {
        for pointer in pointers {
            free(UnsafeMutablePointer(mutating: pointer))
        }
    }
    return try pointers.withUnsafeBufferPointer { try body($0.baseAddress!) }
}

func withOptionalCString<Result>(_ string: String?, _ body: (UnsafePointer<CChar>?) throws -> Result) rethrows -> Result {
    guard let string else {
        return try body(nil)
    }
    return try string.withCString(body)
}

struct UnsafeSendable<Value>: @unchecked Sendable {
    let value: Value
}

struct MessageError: LocalizedError {
    let message: String

    init(consuming message: UnsafeMutablePointer<CChar>?) {
        self.message = message.map { String(cString: $0) } ?? ""
        free(message)
    }

    var errorDescription: String? {
        message
    }
}

final class IORequest<Value: Sendable>: @unchecked Sendable {
    private var continuation: CheckedContinuation<Value, any Error>?
    private var cancelled = false

    func submit(
        on queue: DispatchQueue,
        enqueue: @escaping @Sendable () -> Void,
        withdraw: @escaping @Sendable () -> Void
    ) async throws -> Value {
        try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { continuation in
                queue.async {
                    if self.cancelled {
                        continuation.resume(throwing: CancellationError())
                        return
                    }
                    self.continuation = continuation
                    enqueue()
                }
            }
        } onCancel: {
            queue.async {
                self.cancelled = true
                if self.continuation != nil {
                    withdraw()
                    self.complete(.failure(CancellationError()))
                }
            }
        }
    }

    func complete(_ result: Result<Value, any Error>) {
        continuation?.resume(with: result)
        continuation = nil
    }
}

final class DispatchEventSource {
    private let source: DispatchSourceProtocol
    private var active = false

    init(_ source: DispatchSourceProtocol) {
        self.source = source
    }

    func activate() {
        if !active {
            active = true
            source.resume()
        }
    }

    func deactivate() {
        if active {
            active = false
            source.suspend()
        }
    }

    func cancel() {
        source.cancel()
        activate()
    }
}
