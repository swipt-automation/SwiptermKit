import CSwiptermKit
import Foundation
import os

public struct LinuxRootImageError: LocalizedError, Sendable {
    public let message: String

    public var errorDescription: String? {
        message
    }
}

public struct LinuxRootImage: Sendable {
    fileprivate enum Source: Sendable {
        case file(URL)
        case data(Data)
    }

    private struct Metadata: Decodable {
        let version: Int
    }

    public static let defaultDiskSize: UInt64 = 64 << 30

    private static let metadataName = "swipterm.json"
    fileprivate static let diskName = "root.ext4"

    fileprivate let source: Source

    public static func archive(at url: URL) -> LinuxRootImage {
        LinuxRootImage(source: .file(url))
    }

    public static func archive(data: Data) -> LinuxRootImage {
        LinuxRootImage(source: .data(data))
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

    /// Replaces an image already installed at `root` once the new one is complete.
    /// `diskSize` is the capacity of the sparse disk the guest sees; the host stores only the blocks in use.
    /// Throws when `root` is used by a running machine.
    public func install(
        to root: URL,
        diskSize: UInt64 = defaultDiskSize,
        progress: (@Sendable (Double) -> Void)? = nil
    ) async throws {
        let builder = try LinuxRootImageBuilder(root: root, diskSize: diskSize)
        try await builder.add(self) { progress?($0 * 0.9) }
        try await builder.finish()
        progress?(1)
    }

    public static func export(from root: URL, to archive: URL, progress: (@Sendable (Double) -> Void)? = nil) async throws {
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
        let report: (@Sendable (Double) -> Void)?
        let cancelled = OSAllocatedUnfairLock(initialState: false)
        var reported = -1
        var stopped = false

        init(_ report: (@Sendable (Double) -> Void)?) {
            self.report = report
        }
    }

    fileprivate static func run(
        _ progress: (@Sendable (Double) -> Void)?,
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
                        let percent = Int(fraction * 100)
                        if let report = reporter.report, percent != reporter.reported {
                            reporter.reported = percent
                            report(fraction)
                        }
                        return true
                    }, &message)
                }
                guard succeeded else {
                    defer { free(message) }
                    if reporter.stopped {
                        throw CancellationError()
                    }
                    throw LinuxRootImageError(message: message.map { String(cString: $0) } ?? "")
                }
            }
        } onCancel: {
            reporter.cancelled.withLock { $0 = true }
        }
    }
}

/// The image is installed by `finish()`, replacing an image already installed at `root`; a builder that is released
/// before then leaves `root` unchanged. A later archive or package replaces the entries of earlier ones.
/// A module directory under `/lib/modules` or `/usr/lib/modules` whose `modules.dep` is missing or names a module file
/// it does not contain gets `modules.dep`, `modules.softdep` and `modules.alias` generated from its modules, and loses
/// its `.bin` indexes.
public final class LinuxRootImageBuilder: @unchecked Sendable {
    private let root: URL
    private let handle: OpaquePointer

    /// Throws when `root` is used by a running machine.
    public init(root: URL, diskSize: UInt64 = LinuxRootImage.defaultDiskSize) throws {
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try LinuxRootImage.protect(root)
        var message: UnsafeMutablePointer<CChar>?
        guard let handle = root.withUnsafeFileSystemRepresentation({ swipterm_root_builder_create($0, diskSize, &message) }) else {
            defer { free(message) }
            throw LinuxRootImageError(message: message.map { String(cString: $0) } ?? "")
        }
        self.root = root
        self.handle = handle
    }

    deinit {
        swipterm_root_builder_free(handle)
    }

    public func add(_ image: LinuxRootImage, progress: (@Sendable (Double) -> Void)? = nil) async throws {
        let source = image.source
        try await LinuxRootImage.run(progress) { context, report, message in
            withExtendedLifetime(self) {
                switch source {
                case .file(let url):
                    url.withUnsafeFileSystemRepresentation { path in
                        swipterm_root_builder_add_file(self.handle, path, context, report, message)
                    }
                case .data(let data):
                    data.withUnsafeBytes { buffer in
                        swipterm_root_builder_add_memory(self.handle, buffer.baseAddress, buffer.count, context, report, message)
                    }
                }
            }
        }
    }

    /// Installs the files of a Debian, RPM or tar-based package such as a pacman, APK or XBPS package, without running
    /// its scripts. The top-level files of a tar-based package are its metadata and are not installed.
    public func addPackage(at url: URL, progress: (@Sendable (Double) -> Void)? = nil) async throws -> LinuxPackageContents {
        let collector = PackageCollector()
        try await LinuxRootImage.run(progress) { context, report, message in
            collector.reportContext = context
            collector.report = report
            var callbacks = swipterm_package_callbacks(
                context: Unmanaged.passUnretained(collector).toOpaque(),
                progress: { context, fraction in
                    let collector = Unmanaged<PackageCollector>.fromOpaque(context!).takeUnretainedValue()
                    return collector.report?(collector.reportContext, fraction) ?? true
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

    /// The contents of the regular file at `path`, following symbolic links; `nil` when nothing exists there.
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

    /// Creates missing parent directories.
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
                    defer { free(message) }
                    throw LinuxRootImageError(message: message.map { String(cString: $0) } ?? "")
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
        /// An absolute path; `/` is the package root.
        public let path: String
        public let type: LinuxFileType
        public let permissions: UInt32
        /// The SHA-1 digest of a symbolic link's target, or of the contents a regular file or hard link entry carries; `nil` for a
        /// hard link entry that carries none.
        public let sha1: Data?
    }

    /// The members of a Debian package's control archive, or the top-level files of a tar-based package, by name.
    public let metadata: [String: Data]
    /// The installed entries in archive order.
    public let entries: [Entry]
}
