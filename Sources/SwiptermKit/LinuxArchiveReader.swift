import CSwiptermKit
import Foundation

public struct LinuxArchiveError: LocalizedError, Sendable {
    public let message: String

    public var errorDescription: String? {
        message
    }
}

/// Reads the regular files of a tar archive, or a single file that is not an archive, optionally compressed with
/// gzip, bzip2, xz or zstd.
public final class LinuxArchiveReader {
    private let handle: OpaquePointer

    public init(contentsOf url: URL) throws {
        var message: UnsafeMutablePointer<CChar>?
        guard let handle = url.withUnsafeFileSystemRepresentation({ swipterm_archive_open($0, &message) }) else {
            throw Self.error(message)
        }
        self.handle = handle
    }

    deinit {
        swipterm_archive_close(handle)
    }

    /// The path of the next regular file, or `nil` after the last; a file that is not an archive is one file named `data`.
    public func next() throws -> String? {
        var name: UnsafePointer<CChar>?
        var message: UnsafeMutablePointer<CChar>?
        switch swipterm_archive_next(handle, &name, &message) {
        case 0:
            return nil
        case 1:
            return String(cString: name!)
        default:
            throw Self.error(message)
        }
    }

    /// Up to `count` bytes of the current file; empty at its end.
    public func read(upToCount count: Int) throws -> Data {
        var data = Data(count: count)
        var message: UnsafeMutablePointer<CChar>?
        let read = data.withUnsafeMutableBytes { buffer in
            swipterm_archive_read(handle, buffer.baseAddress, count, &message)
        }
        guard read >= 0 else {
            throw Self.error(message)
        }
        data.count = read
        return data
    }

    public func readToEnd() throws -> Data {
        var contents = Data()
        while true {
            let chunk = try read(upToCount: 1 << 20)
            if chunk.isEmpty {
                return contents
            }
            contents.append(chunk)
        }
    }

    private static func error(_ message: UnsafeMutablePointer<CChar>?) -> LinuxArchiveError {
        defer { free(message) }
        return LinuxArchiveError(message: message.map { String(cString: $0) } ?? "")
    }
}
