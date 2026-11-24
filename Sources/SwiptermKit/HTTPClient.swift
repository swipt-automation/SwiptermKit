import Foundation
import LibSwipterm

public struct HTTPRequest: Sendable {
    public var url: URL
    public var method: String
    public var headers: [String: String]
    public var body: Data?
    /// A value that is not positive disables the timeout.
    public var timeout: Duration

    public init(
        url: URL,
        method: String = "GET",
        headers: [String: String] = [:],
        body: Data? = nil,
        timeout: Duration = .seconds(60)
    ) {
        self.url = url
        self.method = method
        self.headers = headers
        self.body = body
        self.timeout = timeout
    }
}

public struct HTTPResponse: Sendable {
    /// The URL after redirects.
    public let url: URL
    public let statusCode: Int
    public let headers: [String: String]

    init(_ response: LibswiptermHTTPResponse, requestURL: URL) {
        url = URL(string: response.url()) ?? requestURL
        statusCode = Int(response.statusCode())
        var headers: [String: String] = [:]
        for index in 0 ..< response.headerCount() {
            headers[response.headerName(index)] = response.headerValue(index)
        }
        self.headers = headers
    }

    public func value(forHTTPHeaderField field: String) -> String? {
        headers.first { $0.key.caseInsensitiveCompare(field) == .orderedSame }?.value
    }
}

public struct HTTPDownloadProgress: Sendable {
    public struct Source: Sendable {
        public let url: URL
        public let bytesWritten: Int64
        public let activeStreams: Int
        public let failed: Bool
    }

    public let bytesWritten: Int64
    public let totalBytes: Int64?
    public let activeStreams: Int
    /// Sources that are transferring or have written bytes, in the order they joined.
    public let sources: [Source]

    init(_ progress: LibswiptermHTTPDownloadProgress, urls: [URL]) {
        bytesWritten = progress.written()
        totalBytes = progress.total() < 0 ? nil : progress.total()
        activeStreams = Int(progress.streams())
        sources = (0 ..< progress.sourceCount()).map { index in
            Source(
                url: urls[Int(progress.sourceIndex(index))],
                bytesWritten: progress.sourceWritten(index),
                activeStreams: Int(progress.sourceStreams(index)),
                failed: progress.sourceFailed(index)
            )
        }
    }
}

public enum HTTPError: Error, Hashable, Sendable {
    case unexpectedStatus(Int)
    case contentLengthMismatch(expected: Int64, actual: Int64)
    case emptyBody

    init?(_ failure: LibswiptermHTTPFailure) {
        switch failure.kind {
        case LibswiptermHTTPFailureStatus:
            self = .unexpectedStatus(Int(failure.status))
        case LibswiptermHTTPFailureContentLength:
            self = .contentLengthMismatch(expected: failure.expectedBytes, actual: failure.actualBytes)
        case LibswiptermHTTPFailureEmptyBody:
            self = .emptyBody
        default:
            return nil
        }
    }
}

extension HTTPError: LocalizedError {
    public var errorDescription: String? {
        switch self {
        case let .unexpectedStatus(code):
            "HTTP status \(code) \(HTTPURLResponse.localizedString(forStatusCode: code))"
        case let .contentLengthMismatch(expected, actual):
            "Content-Length mismatch: expected \(expected), got \(actual)"
        case .emptyBody:
            "Empty response body"
        }
    }
}

public enum HTTPClient {
    public static func data(for request: HTTPRequest) async throws -> (Data, HTTPResponse) {
        try await execute(request, readBody: true)
    }

    /// Returns when the response header arrives, without reading the body.
    public static func response(for request: HTTPRequest) async throws -> HTTPResponse {
        try await execute(request, readBody: false).1
    }

    private static func execute(_ request: HTTPRequest, readBody: Bool) async throws -> (Data, HTTPResponse) {
        var error: NSError?
        guard let call = LibswiptermNewHTTPRequest(request.method, request.url.absoluteString, request.body, &error) else {
            throw error!
        }
        for (name, value) in request.headers {
            call.setHeader(name, value: value)
        }
        let milliseconds = (request.timeout / .milliseconds(1)).rounded(.up)
        let timeout = milliseconds <= 0 ? 0 : milliseconds >= Double(Int64.max) ? Int64.max : Int64(milliseconds)
        let handle = UnsafeSendable(value: call)
        return try await perform(cancel: { handle.value.cancel() }) {
            do {
                let response = try handle.value.execute(timeout, readBody: readBody)
                return (response.body() ?? Data(), HTTPResponse(response, requestURL: request.url))
            } catch {
                throw handle.value.failure().flatMap(HTTPError.init) ?? error
            }
        }
    }

    /// A failure before the first byte throws immediately; a transfer interrupted after it resumes where it stopped.
    /// `destination` is replaced only by a complete file; a failed download removes its partial data.
    public static func download(
        from url: URL,
        to destination: URL,
        expectedBytes: Int64? = nil,
        progress: @escaping @Sendable (_ bytesWritten: Int64, _ totalBytes: Int64?) -> Void = { _, _ in }
    ) async throws {
        try await download(
            from: [url],
            to: destination,
            expectedBytes: expectedBytes,
            progress: { progress($0.bytesWritten, $0.totalBytes) }
        )
    }

    /// Every URL must serve the same file. With more than one URL, the URLs that support byte ranges download
    /// disjoint ranges concurrently, and a range that fails on one URL is downloaded from the others.
    /// A failure to write the destination throws `POSIXError`.
    public static func download(
        from urls: [URL],
        to destination: URL,
        expectedBytes: Int64? = nil,
        progress: @escaping @Sendable (HTTPDownloadProgress) -> Void = { _ in }
    ) async throws {
        let call = LibswiptermNewHTTPDownload(
            urls[0].absoluteString,
            destination.path(percentEncoded: false),
            expectedBytes ?? 0
        )!
        for url in urls.dropFirst() {
            call.addURL(url.absoluteString)
        }
        let handle = UnsafeSendable(value: call)
        let handler = DownloadHandler(urls: urls, onProgress: progress)
        try await perform(cancel: { handle.value.cancel() }) {
            do {
                try handle.value.run(handler)
            } catch {
                let failure = handle.value.failure()
                if let failure, failure.kind == LibswiptermHTTPFailureFile, let code = POSIXErrorCode(rawValue: failure.errorNumber) {
                    throw POSIXError(code, userInfo: [NSLocalizedDescriptionKey: error.localizedDescription])
                }
                throw failure.flatMap(HTTPError.init) ?? error
            }
        }
    }

    private static func perform<Result: Sendable>(
        cancel: @escaping @Sendable () -> Void,
        _ body: @escaping @Sendable () throws -> Result
    ) async throws -> Result {
        do {
            return try await withTaskCancellationHandler {
                try await blocking(body)
            } onCancel: {
                cancel()
            }
        } catch {
            try Task.checkCancellation()
            throw error
        }
    }
}

private final class DownloadHandler: NSObject, LibswiptermHTTPDownloadHandlerProtocol, Sendable {
    private let urls: [URL]
    private let onProgress: @Sendable (HTTPDownloadProgress) -> Void

    init(urls: [URL], onProgress: @escaping @Sendable (HTTPDownloadProgress) -> Void) {
        self.urls = urls
        self.onProgress = onProgress
    }

    func progress(_ progress: LibswiptermHTTPDownloadProgress?) {
        onProgress(HTTPDownloadProgress(progress!, urls: urls))
    }
}
