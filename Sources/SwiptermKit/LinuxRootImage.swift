import CSwiptermKit
import Foundation
import os

public struct LinuxRootImage: Sendable {
    private struct Metadata: Decodable {
        let version: Int
    }

    private static let metadataName = "swipterm.json"
    fileprivate static let diskName = "root.ext4"

    fileprivate let url: URL

    public static func archive(at url: URL) -> LinuxRootImage {
        LinuxRootImage(url: url)
    }

    public static func isInstalled(at root: URL) -> Bool {
        guard let data = try? Data(contentsOf: root.appending(path: metadataName)),
              let metadata = try? JSONDecoder().decode(Metadata.self, from: data),
              metadata.version == 2
        else {
            return false
        }
        return FileManager.default.fileExists(atPath: root.appending(path: diskName).path(percentEncoded: false))
    }

    public static func installedKernels(at root: URL) throws -> [String] {
        var releases: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?
        var count = 0
        try root.withUnsafeFileSystemRepresentation { rootPath in
            try check(swipterm_root_kernels(rootPath, &releases, &count))
        }
        defer { swipterm_free_strings(releases, count) }
        return (0 ..< count).map { String(cString: releases![$0]!) }
    }

    public static func contents(of path: String, at root: URL) throws -> Data? {
        var data: UnsafeMutableRawPointer?
        var size = 0
        let code = root.withUnsafeFileSystemRepresentation { swipterm_root_read($0, path, &data, &size) }
        if code == -LinuxError.ENOENT.rawValue {
            return nil
        }
        try check(code)
        return Data(bytesNoCopy: data!, count: size, deallocator: .free)
    }

    public static func export(from root: URL, to archive: URL, progress: @escaping @Sendable (Double) -> Void) async throws {
        try await run(progress) { context, report, message in
            root.withUnsafeFileSystemRepresentation { rootPath in
                archive.withUnsafeFileSystemRepresentation { archivePath in
                    swipterm_root_export(rootPath, archivePath, context, report, message)
                }
            }
        }
    }

    fileprivate static func protect(_ url: URL) throws {
        var values = URLResourceValues()
        values.isExcludedFromBackup = true
        var url = url
        try url.setResourceValues(values)
        #if os(iOS)
            try FileManager.default.setAttributes(
                [.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication],
                ofItemAtPath: url.path(percentEncoded: false)
            )
        #endif
    }

    fileprivate final class ProgressReporter: @unchecked Sendable {
        let report: @Sendable (Double) -> Void
        let cancelled = OSAllocatedUnfairLock(initialState: false)
        var reported = -1.0
        var nextReport = ContinuousClock.now
        var stopped = false

        init(_ report: @escaping @Sendable (Double) -> Void) {
            self.report = report
        }
    }

    fileprivate static func run(
        _ progress: @escaping @Sendable (Double) -> Void,
        _ body: @escaping @Sendable (
            UnsafeMutableRawPointer?,
            swipterm_progress_handler?,
            UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>
        ) -> Bool
    ) async throws {
        let reporter = ProgressReporter(progress)
        try await withTaskCancellationHandler {
            try await blocking {
                var message: UnsafeMutablePointer<CChar>?
                let succeeded = withExtendedLifetime(reporter) {
                    body(Unmanaged.passUnretained(reporter).toOpaque(), { context, fraction in
                        let reporter = Unmanaged<ProgressReporter>.fromOpaque(context!).takeUnretainedValue()
                        if fraction < 1, reporter.cancelled.withLock({ $0 }) {
                            reporter.stopped = true
                            return false
                        }
                        if fraction != reporter.reported {
                            let now = ContinuousClock.now
                            if reporter.reported < 0 || fraction >= 1 || now >= reporter.nextReport {
                                reporter.reported = fraction
                                reporter.nextReport = now.advanced(by: .milliseconds(100))
                                reporter.report(fraction)
                            }
                        }
                        return true
                    }, &message)
                }
                guard succeeded else {
                    let error = MessageError(consuming: message)
                    if reporter.stopped {
                        throw CancellationError()
                    }
                    throw error
                }
            }
        } onCancel: {
            reporter.cancelled.withLock { $0 = true }
        }
    }
}

public final class LinuxRootImageBuilder: @unchecked Sendable {
    private static let diskSize: UInt64 = 64 << 30

    private let root: URL
    private let handle: OpaquePointer

    public init(root: URL) throws {
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try LinuxRootImage.protect(root)
        var message: UnsafeMutablePointer<CChar>?
        guard let handle = root.withUnsafeFileSystemRepresentation({ swipterm_root_builder_create($0, Self.diskSize, &message) }) else {
            throw MessageError(consuming: message)
        }
        self.root = root
        self.handle = handle
    }

    deinit {
        swipterm_root_builder_free(handle)
    }

    public func add(_ image: LinuxRootImage, progress: @escaping @Sendable (Double) -> Void) async throws {
        let url = image.url
        try await LinuxRootImage.run(progress) { context, report, message in
            withExtendedLifetime(self) {
                url.withUnsafeFileSystemRepresentation { path in
                    swipterm_root_builder_add_file(self.handle, path, context, report, message)
                }
            }
        }
    }

    public func addPackage(at url: URL, progress: @escaping @Sendable (Double) -> Void) async throws -> LinuxPackageContents {
        let collector = PackageCollector()
        try await LinuxRootImage.run(progress) { context, report, message in
            collector.reportContext = context
            collector.report = report
            var callbacks = swipterm_package_callbacks(
                context: Unmanaged.passUnretained(collector).toOpaque(),
                progress: { context, fraction in
                    let collector = Unmanaged<PackageCollector>.fromOpaque(context!).takeUnretainedValue()
                    return collector.report!(collector.reportContext, fraction)
                },
                metadata: { context, name, data, size in
                    let collector = Unmanaged<PackageCollector>.fromOpaque(context!).takeUnretainedValue()
                    collector.metadata[String(cString: name!)] = data.map { Data(bytes: $0, count: size) } ?? Data()
                },
                entry: { context, path, mode, sha1 in
                    let collector = Unmanaged<PackageCollector>.fromOpaque(context!).takeUnretainedValue()
                    collector.entries.append(LinuxPackageContents.Entry(
                        path: String(cString: path!),
                        type: LinuxFileType(mode: mode),
                        permissions: mode & 0o7777,
                        sha1: sha1.map { Data(bytes: $0, count: 20) }
                    ))
                }
            )
            return withExtendedLifetime((self, collector)) {
                url.withUnsafeFileSystemRepresentation { path in
                    swipterm_root_builder_add_package(self.handle, path, &callbacks, message)
                }
            }
        }
        return LinuxPackageContents(metadata: collector.metadata, entries: collector.entries)
    }

    public func contents(of path: String) throws -> Data? {
        var data: UnsafeMutableRawPointer?
        var size = 0
        let code = swipterm_root_builder_read(handle, path, &data, &size)
        if code == -LinuxError.ENOENT.rawValue {
            return nil
        }
        try check(code)
        return Data(bytesNoCopy: data!, count: size, deallocator: .free)
    }

    public func contentsOfDirectory(at path: String) throws -> [String]? {
        var names: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?
        var count = 0
        let code = swipterm_root_builder_list(handle, path, &names, &count)
        if code == -LinuxError.ENOENT.rawValue {
            return nil
        }
        try check(code)
        defer { swipterm_free_strings(names, count) }
        return (0 ..< count).map { String(cString: names![$0]!) }
    }

    public func write(_ contents: Data, to path: String, permissions: UInt32 = 0o644) throws {
        let code = contents.withUnsafeBytes { buffer in
            swipterm_root_builder_write(handle, path, buffer.baseAddress, buffer.count, permissions)
        }
        try check(code)
    }

    public func finish() async throws {
        try await blocking {
            try withExtendedLifetime(self) {
                var message: UnsafeMutablePointer<CChar>?
                guard swipterm_root_builder_finish(self.handle, &message) else {
                    throw MessageError(consuming: message)
                }
            }
        }
        try LinuxRootImage.protect(root.appending(path: LinuxRootImage.diskName))
    }

    private final class PackageCollector: @unchecked Sendable {
        var reportContext: UnsafeMutableRawPointer?
        var report: swipterm_progress_handler?
        var metadata: [String: Data] = [:]
        var entries: [LinuxPackageContents.Entry] = []
    }
}

public struct LinuxPackageContents: Sendable {
    public struct Entry: Hashable, Sendable {
        public let path: String
        public let type: LinuxFileType
        public let permissions: UInt32
        public let sha1: Data?
    }

    public let metadata: [String: Data]
    public let entries: [Entry]
}
