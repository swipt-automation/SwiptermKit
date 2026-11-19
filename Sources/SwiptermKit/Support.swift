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

struct UnsafeSendable<Value>: @unchecked Sendable {
    let value: Value
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
