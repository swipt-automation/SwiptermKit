import CSwiptermKit
import Foundation
import Network

public struct LinuxError: LocalizedError {
    public let errno: Int32

    init(code: Int32) {
        errno = -code
    }

    public var errorDescription: String? {
        "Linux errno \(errno)"
    }
}

public struct RootImportError: LocalizedError {
    public let message: String

    public var errorDescription: String? {
        message
    }
}

public enum LinuxKernel {
    public static let defaultEnvironment = [
        "TERM=xterm-256color",
        "HOME=/root",
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
        "PYTHONMALLOC=malloc",
    ]

    public struct Configuration: Sendable {
        public var root: URL
        public var initArguments: [String]
        public var environment: [String]
        public var version: String?
        public var consoleOutput: (@Sendable (Data) -> Void)?

        public init(
            root: URL,
            initArguments: [String] = ["/sbin/init"],
            environment: [String] = LinuxKernel.defaultEnvironment,
            version: String? = nil,
            consoleOutput: (@Sendable (Data) -> Void)? = nil
        ) {
            self.root = root
            self.initArguments = initArguments
            self.environment = environment
            self.version = version
            self.consoleOutput = consoleOutput
        }
    }

    private static let pathMonitor = NWPathMonitor()

    public static func importRoot(
        from archive: URL,
        to root: URL,
        progress: @escaping @Sendable (Double) -> Void = { _ in }
    ) async throws {
        final class Reporter {
            let report: @Sendable (Double) -> Void
            var reported = -1

            init(_ report: @escaping @Sendable (Double) -> Void) {
                self.report = report
            }
        }
        let temporary = URL.temporaryDirectory.appending(path: UUID().uuidString)
        let reporter = Reporter(progress)
        var message: UnsafeMutablePointer<CChar>?
        let imported = withExtendedLifetime(reporter) {
            swipterm_root_import(
                archive.path(percentEncoded: false),
                temporary.path(percentEncoded: false),
                Unmanaged.passUnretained(reporter).toOpaque(),
                { context, fraction in
                    let reporter = Unmanaged<Reporter>.fromOpaque(context!).takeUnretainedValue()
                    let percent = Int(fraction * 100)
                    guard percent != reporter.reported else { return }
                    reporter.reported = percent
                    reporter.report(fraction)
                },
                &message
            )
        }
        guard imported else {
            try? FileManager.default.removeItem(at: temporary)
            defer { free(message) }
            throw RootImportError(message: message.map { String(cString: $0) } ?? "")
        }
        try FileManager.default.createDirectory(at: root.deletingLastPathComponent(), withIntermediateDirectories: true)
        try FileManager.default.moveItem(at: temporary, to: root)
    }

    public static func boot(_ configuration: Configuration) async throws {
        let console = configuration.consoleOutput.map { Unmanaged.passRetained(OutputHandler($0)) }
        let rootPath = strdup(configuration.root.path(percentEncoded: false))
        defer { free(rootPath) }
        #if targetEnvironment(simulator)
            let socketPrefix: UnsafeMutablePointer<CChar>? = nil
        #else
            let socketPrefix = strdup(URL.temporaryDirectory.appending(path: "ishsock").path(percentEncoded: false))
        #endif
        defer { free(socketPrefix) }
        let version = configuration.version.flatMap { strdup($0) }
        defer { free(version) }
        let err = withCStringArray(configuration.initArguments) { argv in
            withCStringArray(configuration.environment) { envp in
                var options = swipterm_options(
                    root_path: rootPath,
                    socket_prefix: socketPrefix,
                    version: version,
                    init_argv: argv,
                    init_envp: envp,
                    console_context: console?.toOpaque(),
                    console_output: console == nil ? nil : { context, data, length in
                        Unmanaged<OutputHandler>.fromOpaque(context!).takeUnretainedValue()
                            .handle(Data(bytes: data!, count: length))
                    }
                )
                return swipterm_boot(&options)
            }
        }
        guard err == 0 else {
            console?.release()
            throw LinuxError(code: err)
        }
        pathMonitor.pathUpdateHandler = { _ in
            _ = swipterm_update_resolv_conf()
        }
        pathMonitor.start(queue: DispatchQueue(label: "SwiptermKit.PathMonitor"))
    }

    public static func readFile(_ path: String) throws -> Data {
        var data: UnsafeMutableRawPointer?
        var length = 0
        let err = swipterm_read_file(path, &data, &length)
        guard err == 0 else {
            throw LinuxError(code: err)
        }
        return Data(bytesNoCopy: data!, count: length, deallocator: .free)
    }

    public static func writeFile(_ path: String, contents: Data) throws {
        let err = contents.withUnsafeBytes { buffer in
            swipterm_write_file(path, buffer.baseAddress, buffer.count)
        }
        guard err == 0 else {
            throw LinuxError(code: err)
        }
    }
}

final class OutputHandler {
    let handle: @Sendable (Data) -> Void

    init(_ handle: @escaping @Sendable (Data) -> Void) {
        self.handle = handle
    }
}

func withCStringArray<Result>(_ strings: [String], _ body: (UnsafePointer<UnsafePointer<CChar>?>) throws -> Result) rethrows -> Result {
    let pointers = strings.map { UnsafePointer(strdup($0)) } + [nil]
    defer {
        for pointer in pointers {
            free(UnsafeMutablePointer(mutating: pointer))
        }
    }
    return try pointers.withUnsafeBufferPointer { try body($0.baseAddress!) }
}
