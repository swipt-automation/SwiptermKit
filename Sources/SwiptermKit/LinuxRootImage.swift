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
    private enum Source: Sendable {
        case file(URL)
        case data(Data)
    }

    private struct Metadata: Decodable {
        let version: Int
    }

    public static let defaultDiskSize: UInt64 = 64 << 30

    private static let metadataName = "swipterm.json"
    private static let diskName = "root.ext4"

    private let source: Source

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
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        try Self.protect(root)
        let source = source
        try await Self.run(progress) { context, report, message in
            root.withUnsafeFileSystemRepresentation { rootPath in
                switch source {
                case .file(let url):
                    url.withUnsafeFileSystemRepresentation { archivePath in
                        swipterm_root_import_file(archivePath, rootPath, diskSize, context, report, message)
                    }
                case .data(let data):
                    data.withUnsafeBytes { buffer in
                        swipterm_root_import_memory(buffer.baseAddress, buffer.count, rootPath, diskSize, context, report, message)
                    }
                }
            }
        }
        try Self.protect(root.appending(path: Self.diskName))
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

    private static func protect(_ url: URL) throws {
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

    private final class ProgressReporter: @unchecked Sendable {
        let report: (@Sendable (Double) -> Void)?
        let cancelled = OSAllocatedUnfairLock(initialState: false)
        var reported = -1
        var stopped = false

        init(_ report: (@Sendable (Double) -> Void)?) {
            self.report = report
        }
    }

    private static func run(
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
