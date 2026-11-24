import Foundation
import LibSwipterm

extension LibswiptermHTTPRequest: @retroactive @unchecked Sendable {}
extension LibswiptermHTTPResponse: @retroactive @unchecked Sendable {}

public enum HTTPClient {
    public static func data(for request: URLRequest) async throws -> (Data, HTTPURLResponse) {
        let (body, response) = try await bytes(for: request)
        return try await (body.readToEnd(), response)
    }

    public static func data(from url: URL) async throws -> (Data, HTTPURLResponse) {
        try await data(for: URLRequest(url: url))
    }

    public static func bytes(for request: URLRequest) async throws -> (HTTPBody, HTTPURLResponse) {
        let (call, response, httpResponse) = try await execute(request)
        return (HTTPBody(request: call, response: response), httpResponse)
    }

    public static func bytes(from url: URL) async throws -> (HTTPBody, HTTPURLResponse) {
        try await bytes(for: URLRequest(url: url))
    }

    /// `destination` is replaced only after the whole body is written.
    public static func download(
        for request: URLRequest,
        to destination: URL,
        progress: (@Sendable (_ bytesWritten: Int64, _ totalBytes: Int64?) -> Void)? = nil
    ) async throws -> HTTPURLResponse {
        let (call, response, httpResponse) = try await execute(request)
        let handler = progress.map { DownloadHandler(onProgress: $0) }
        try await perform(call) {
            try response.download(destination.path(percentEncoded: false), handler: handler)
        }
        return httpResponse
    }

    /// `destination` is replaced only after the whole body is written.
    public static func download(
        from url: URL,
        to destination: URL,
        progress: (@Sendable (_ bytesWritten: Int64, _ totalBytes: Int64?) -> Void)? = nil
    ) async throws -> HTTPURLResponse {
        try await download(for: URLRequest(url: url), to: destination, progress: progress)
    }

    private static func execute(
        _ request: URLRequest
    ) async throws -> (LibswiptermHTTPRequest, LibswiptermHTTPResponse, HTTPURLResponse) {
        var error: NSError?
        guard let call = LibswiptermNewHTTPRequest(
            request.httpMethod ?? "GET",
            request.url?.absoluteString ?? "",
            request.httpBody,
            request.timeoutInterval,
            &error
        ) else {
            throw error!
        }
        for (name, value) in request.allHTTPHeaderFields ?? [:] {
            call.setHeader(name, value: value)
        }
        let response = try await perform(call) { try call.execute() }
        var headers: [String: String] = [:]
        for index in 0 ..< response.headerCount() {
            headers[response.headerName(index)] = response.headerValue(index)
        }
        let httpResponse = HTTPURLResponse(
            url: URL(string: response.url())!,
            statusCode: Int(response.statusCode()),
            httpVersion: response.proto(),
            headerFields: headers
        )!
        return (call, response, httpResponse)
    }

    private static func perform<Result: Sendable>(
        _ call: LibswiptermHTTPRequest,
        _ body: @escaping @Sendable () throws -> Result
    ) async throws -> Result {
        do {
            return try await withTaskCancellationHandler {
                try await blocking(body)
            } onCancel: {
                call.cancel()
            }
        } catch {
            throw call.failure(error)
        }
    }
}

public final class HTTPBody: Sendable {
    private let request: LibswiptermHTTPRequest
    private let response: LibswiptermHTTPResponse
    private let queue = DispatchQueue(label: "SwiptermKit.HTTPBody")

    init(request: LibswiptermHTTPRequest, response: LibswiptermHTTPResponse) {
        self.request = request
        self.response = response
    }

    deinit {
        response.close()
    }

    public func read(maxLength: Int = 65536) async throws -> Data? {
        precondition(maxLength > 0)
        let data: Data?
        do {
            data = try await withTaskCancellationHandler {
                try await withCheckedThrowingContinuation { continuation in
                    queue.async {
                        continuation.resume(with: Result {
                            try self.response.read(Int64(maxLength))
                            return self.response.chunk()
                        })
                    }
                }
            } onCancel: {
                close()
            }
        } catch {
            throw request.failure(error)
        }
        if data == nil {
            try Task.checkCancellation()
        }
        return data
    }

    public func readToEnd() async throws -> Data {
        var data = Data()
        while let chunk = try await read() {
            data.append(chunk)
        }
        return data
    }

    public func close() {
        response.close()
    }
}

extension HTTPBody: AsyncSequence {
    public typealias Element = Data

    public struct AsyncIterator: AsyncIteratorProtocol {
        let body: HTTPBody

        public mutating func next() async throws -> Data? {
            try await body.read()
        }
    }

    public func makeAsyncIterator() -> AsyncIterator {
        AsyncIterator(body: self)
    }
}

private extension LibswiptermHTTPRequest {
    func failure(_ error: Error) -> Error {
        if Task.isCancelled {
            return CancellationError()
        }
        if timedOut() {
            return URLError(.timedOut)
        }
        if let code = POSIXErrorCode(rawValue: errorNumber()) {
            return POSIXError(code, userInfo: [NSLocalizedDescriptionKey: error.localizedDescription])
        }
        return error
    }
}

private final class DownloadHandler: NSObject, LibswiptermHTTPDownloadHandlerProtocol, Sendable {
    private let onProgress: @Sendable (_ bytesWritten: Int64, _ totalBytes: Int64?) -> Void

    init(onProgress: @escaping @Sendable (_ bytesWritten: Int64, _ totalBytes: Int64?) -> Void) {
        self.onProgress = onProgress
    }

    func progress(_ written: Int64, total: Int64) {
        onProgress(written, total < 0 ? nil : total)
    }
}
