import CSwiptermKit
import Foundation

public enum LinuxFileType: Hashable, Sendable {
    case regular
    case directory
    case symbolicLink
    case characterDevice
    case blockDevice
    case fifo
    case socket
    case unknown

    init(mode: UInt32) {
        self = switch mode & 0o170000 {
        case 0o100000: .regular
        case 0o040000: .directory
        case 0o120000: .symbolicLink
        case 0o020000: .characterDevice
        case 0o060000: .blockDevice
        case 0o010000: .fifo
        case 0o140000: .socket
        default: .unknown
        }
    }
}

public struct LinuxFileSystem: Sendable {
    private let runtime: LinuxRuntime

    init(runtime: LinuxRuntime) {
        self.runtime = runtime
    }

    public func stat(_ path: String) async throws -> LinuxFileType {
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            var mode: UInt32 = 0
            try check(swipterm_stat_path(runtimeHandle, path, &mode))
            return LinuxFileType(mode: mode)
        }
    }

    public func contents(of path: String) async throws -> Data {
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            var data: UnsafeMutableRawPointer?
            var length = 0
            try check(swipterm_read_file(runtimeHandle, path, &data, &length))
            guard let data else {
                return Data()
            }
            return Data(bytesNoCopy: data, count: length, deallocator: .free)
        }
    }

    public func writeFile(_ path: String, contents: Data, permissions: UInt32 = 0o644) async throws {
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            try check(contents.withUnsafeBytes {
                swipterm_write_file(runtimeHandle, path, $0.baseAddress, $0.count, permissions)
            })
        }
    }

    public func createDirectory(
        _ path: String,
        permissions: UInt32 = 0o755,
        withIntermediateDirectories: Bool
    ) async throws {
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_create_directory(runtimeHandle, path, permissions, withIntermediateDirectories))
        }
    }
}
