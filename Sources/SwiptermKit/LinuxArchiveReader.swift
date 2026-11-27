import CSwiptermKit
import Foundation

public final class LinuxArchiveReader {
    private let handle: OpaquePointer

    public init(contentsOf url: URL) throws {
        var message: UnsafeMutablePointer<CChar>?
        guard let handle = url.withUnsafeFileSystemRepresentation({ swipterm_archive_open($0, &message) }) else {
            throw MessageError(consuming: message)
        }
        self.handle = handle
    }

    deinit {
        swipterm_archive_close(handle)
    }

    public func next() throws -> String? {
        var name: UnsafePointer<CChar>?
        var message: UnsafeMutablePointer<CChar>?
        switch swipterm_archive_next(handle, &name, &message) {
        case 0:
            return nil
        case 1:
            return String(cString: name!)
        default:
            throw MessageError(consuming: message)
        }
    }

    public func read(upToCount count: Int) throws -> Data {
        var data = Data(count: count)
        var message: UnsafeMutablePointer<CChar>?
        let read = data.withUnsafeMutableBytes { buffer in
            swipterm_archive_read(handle, buffer.baseAddress, count, &message)
        }
        guard read >= 0 else {
            throw MessageError(consuming: message)
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
}
