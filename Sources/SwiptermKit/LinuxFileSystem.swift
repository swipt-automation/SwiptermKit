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

public struct LinuxFileStat: Hashable, Sendable {
    public var type: LinuxFileType
    public var permissions: UInt32
    public var userID: UInt32
    public var groupID: UInt32
    public var linkCount: UInt32
    public var size: UInt64
    public var inode: UInt64
    public var deviceID: UInt64
    public var accessDate: Date
    public var modificationDate: Date
    public var changeDate: Date

    init(_ stat: swipterm_stat) {
        func date(_ time: timespec) -> Date {
            Date(timeIntervalSince1970: TimeInterval(time.tv_sec) + TimeInterval(time.tv_nsec) / 1_000_000_000)
        }
        type = LinuxFileType(mode: stat.mode)
        permissions = stat.mode & 0o7777
        userID = stat.uid
        groupID = stat.gid
        linkCount = stat.nlink
        size = stat.size
        inode = stat.inode
        deviceID = stat.rdev
        accessDate = date(stat.access_time)
        modificationDate = date(stat.modification_time)
        changeDate = date(stat.change_time)
    }
}

public struct LinuxDirectoryEntry: Hashable, Sendable {
    public var name: String
    public var type: LinuxFileType

    public init(name: String, type: LinuxFileType) {
        self.name = name
        self.type = type
    }
}

public struct LinuxFileSystem: Sendable {
    private let runtime: LinuxRuntime

    init(runtime: LinuxRuntime) {
        self.runtime = runtime
    }

    public func stat(_ path: String, followSymlinks: Bool = true) async throws -> LinuxFileStat {
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            var stat = swipterm_stat()
            try check(swipterm_stat_path(runtimeHandle, path, followSymlinks, &stat))
            return LinuxFileStat(stat)
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

    public func listDirectory(_ path: String) async throws -> [LinuxDirectoryEntry] {
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            var entries: UnsafeMutablePointer<swipterm_directory_entry>?
            var count = 0
            try check(swipterm_list_directory(runtimeHandle, path, &entries, &count))
            defer { swipterm_free_directory_entries(entries, count) }
            return (0..<count).map {
                LinuxDirectoryEntry(name: String(cString: entries![$0].name), type: LinuxFileType(mode: entries![$0].mode))
            }
        }
    }

    public func createDirectory(
        _ path: String,
        permissions: UInt32 = 0o755,
        withIntermediateDirectories: Bool = false
    ) async throws {
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_create_directory(runtimeHandle, path, permissions, withIntermediateDirectories))
        }
    }

    public func removeItem(_ path: String, recursive: Bool = false) async throws {
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_remove(runtimeHandle, path, recursive))
        }
    }
}
