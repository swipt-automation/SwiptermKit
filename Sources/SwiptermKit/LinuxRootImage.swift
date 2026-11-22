import CSwiptermKit
import Foundation

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
        case daxMemory(UnsafeSendable<UnsafeRawBufferPointer>)
    }

    private let source: Source

    public static func archive(at url: URL) -> LinuxRootImage {
        LinuxRootImage(source: .file(url))
    }

    public static func archive(data: Data) -> LinuxRootImage {
        LinuxRootImage(source: .data(data))
    }

    public static func daxMemory(_ buffer: UnsafeRawBufferPointer) -> LinuxRootImage {
        LinuxRootImage(source: .daxMemory(UnsafeSendable(value: buffer)))
    }

    func withCRootImage<Result>(_ body: (swipterm_root_image) throws -> Result) rethrows -> Result {
        switch source {
        case .file(let url):
            return try url.withUnsafeFileSystemRepresentation { path in
                try body(swipterm_root_image(
                    kind: SWIPTERM_ROOT_IMAGE_ARCHIVE_FILE,
                    path: path,
                    data: nil,
                    size: 0
                ))
            }
        case .data(let data):
            return try data.withUnsafeBytes { buffer in
                try body(swipterm_root_image(
                    kind: SWIPTERM_ROOT_IMAGE_ARCHIVE_MEMORY,
                    path: nil,
                    data: buffer.baseAddress,
                    size: buffer.count
                ))
            }
        case .daxMemory(let buffer):
            return try body(swipterm_root_image(
                kind: SWIPTERM_ROOT_IMAGE_DAX_EROFS_MEMORY,
                path: nil,
                data: buffer.value.baseAddress,
                size: buffer.value.count
            ))
        }
    }

    public func install(to root: URL, progress: (@Sendable (Double) -> Void)? = nil) async throws {
        let parent = root.deletingLastPathComponent()
        try FileManager.default.createDirectory(at: parent, withIntermediateDirectories: true)
        let temporary = parent.appending(path: ".\(root.lastPathComponent).\(UUID().uuidString)", directoryHint: .isDirectory)
        let source = source
        do {
            try await Self.run(progress) { context, report, message in
                temporary.withUnsafeFileSystemRepresentation { rootPath in
                    switch source {
                    case .file(let url):
                        url.withUnsafeFileSystemRepresentation { archivePath in
                            swipterm_image_import_file(archivePath, rootPath, context, report, message)
                        }
                    case .data(let data):
                        data.withUnsafeBytes { buffer in
                            swipterm_image_import_memory(buffer.baseAddress, buffer.count, rootPath, context, report, message)
                        }
                    case .daxMemory(let buffer):
                        swipterm_image_import_memory(buffer.value.baseAddress, buffer.value.count, rootPath, context, report, message)
                    }
                }
            }
            let fm = FileManager.default
            if fm.fileExists(atPath: root.path(percentEncoded: false)) {
                _ = try fm.replaceItemAt(root, withItemAt: temporary)
            } else {
                try fm.moveItem(at: temporary, to: root)
            }
        } catch {
            try? FileManager.default.removeItem(at: temporary)
            throw error
        }
    }

    public static func export(from root: URL, to archive: URL, progress: (@Sendable (Double) -> Void)? = nil) async throws {
        try await run(progress) { context, report, message in
            root.withUnsafeFileSystemRepresentation { rootPath in
                archive.withUnsafeFileSystemRepresentation { archivePath in
                    swipterm_image_export(rootPath, archivePath, context, report, message)
                }
            }
        }
    }

    private final class ProgressReporter {
        let report: @Sendable (Double) -> Void
        var reported = -1

        init(_ report: @escaping @Sendable (Double) -> Void) {
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
        try await blocking {
            let reporter = progress.map(ProgressReporter.init)
            var message: UnsafeMutablePointer<CChar>?
            let succeeded = withExtendedLifetime(reporter) {
                body(reporter.map { Unmanaged.passUnretained($0).toOpaque() }, reporter == nil ? nil : { context, fraction in
                    let reporter = Unmanaged<ProgressReporter>.fromOpaque(context!).takeUnretainedValue()
                    let percent = Int(fraction * 100)
                    guard percent != reporter.reported else { return }
                    reporter.reported = percent
                    reporter.report(fraction)
                }, &message)
            }
            guard succeeded else {
                defer { free(message) }
                throw LinuxRootImageError(message: message.map { String(cString: $0) } ?? "")
            }
        }
    }
}
