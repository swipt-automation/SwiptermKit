import Foundation
import LibSwipterm

extension LibswiptermHTTPRequest: @retroactive @unchecked Sendable {}
extension LibswiptermHTTPResponse: @retroactive @unchecked Sendable {}

public enum HTTPClient {
    public static func data(from url: URL) async throws -> (Data, HTTPURLResponse) {
        let (call, response, httpResponse) = try await execute(url)
        let data = try await perform(call) {
            try response.readAll()
        }
        return (data, httpResponse)
    }

    public static func download(
        from url: URL,
        to destination: URL,
        progress: @escaping @Sendable (_ bytesWritten: Int64, _ totalBytes: Int64?) -> Void
    ) async throws -> HTTPURLResponse {
        let (call, response, httpResponse) = try await execute(url)
        let handler = DownloadHandler(onProgress: progress)
        try await perform(call) {
            try response.download(destination.path(percentEncoded: false), handler: handler)
        }
        return httpResponse
    }

    private static func execute(_ url: URL) async throws -> (LibswiptermHTTPRequest, LibswiptermHTTPResponse, HTTPURLResponse) {
        var error: NSError?
        guard let call = LibswiptermNewHTTPRequest(url.absoluteString, &error) else {
            throw error!
        }
        let response = try await perform(call) { try call.execute() }
        let httpResponse = HTTPURLResponse(
            url: URL(string: response.url())!,
            statusCode: Int(response.statusCode()),
            httpVersion: nil,
            headerFields: nil
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
            if Task.isCancelled {
                throw CancellationError()
            }
            if call.timedOut() {
                throw URLError(.timedOut)
            }
            if let code = POSIXErrorCode(rawValue: call.errorNumber()) {
                throw POSIXError(code, userInfo: [NSLocalizedDescriptionKey: error.localizedDescription])
            }
            throw error
        }
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
