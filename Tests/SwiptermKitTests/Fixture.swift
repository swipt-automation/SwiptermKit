import Foundation
import SwiptermAlpineRootfs
import SwiptermKit
import Testing

enum Fixture {
    static let directory = URL.temporaryDirectory.appending(path: "SwiptermKitTests-\(UUID().uuidString)", directoryHint: .isDirectory)
    static let root = directory.appending(path: "root", directoryHint: .isDirectory)

    private static let booted = Task {
        try await LinuxRuntime.boot(LinuxRuntime.Configuration(
            root: root,
            initialImage: .alpine,
            hostname: "swipterm-tests",
            version: "SwiptermKitTests",
            autoSyncDNS: false
        ))
    }

    static var runtime: LinuxRuntime {
        get async throws {
            try await booted.value
        }
    }
}

struct TimeoutError: Error, CustomStringConvertible {
    let description: String
}

func withTimeout<Result: Sendable>(
    _ seconds: Double,
    _ description: String = "operation",
    _ body: @escaping @Sendable () async throws -> Result
) async throws -> Result {
    try await withThrowingTaskGroup(of: Result.self) { group in
        group.addTask(operation: body)
        group.addTask {
            try await Task.sleep(for: .seconds(seconds))
            throw TimeoutError(description: "\(description) timed out after \(seconds)s")
        }
        defer { group.cancelAll() }
        return try await group.next()!
    }
}

func shell(
    _ script: String,
    input: Data? = nil,
    environment: [String: String]? = nil,
    workingDirectory: String? = nil
) async throws -> LinuxProcess.Output {
    try await Fixture.runtime.run(
        LinuxProcess.Configuration(
            executablePath: "/bin/sh",
            arguments: ["-c", script],
            environment: environment,
            workingDirectory: workingDirectory
        ),
        input: input
    )
}

extension LinuxProcess.Output {
    var text: String {
        String(decoding: standardOutput, as: UTF8.self)
    }

    var errorText: String {
        String(decoding: standardError, as: UTF8.self)
    }
}

final class Signal: Sendable {
    private let stream: AsyncStream<Void>
    private let continuation: AsyncStream<Void>.Continuation

    init() {
        (stream, continuation) = AsyncStream.makeStream()
    }

    func fire() {
        continuation.yield()
    }

    func wait(_ description: String, timeout seconds: Double = 30) async throws {
        let stream = stream
        try await withTimeout(seconds, description) {
            var iterator = stream.makeAsyncIterator()
            _ = await iterator.next()
        }
    }
}
