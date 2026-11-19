import Foundation
import SwiptermAlpineRootfs
import SwiptermKit
import Testing

@Suite struct RootImageTests {
    @Test func exportedRootsImportAgain() async throws {
        let runtime = try await Fixture.runtime
        let marker = "/root/export-\(UUID().uuidString)"
        try await runtime.fileSystem.writeFile(marker, contents: Data("exported".utf8), permissions: 0o640)

        let archive = Fixture.directory.appending(path: "export-\(UUID().uuidString).tar.gz")
        try await LinuxRootImage.export(from: Fixture.root, to: archive)

        let fromFile = Fixture.directory.appending(path: "import-file-\(UUID().uuidString)", directoryHint: .isDirectory)
        let progress = ProgressRecorder()
        try await LinuxRootImage.archive(at: archive).install(to: fromFile) { progress.record($0) }
        #expect(progress.values.isEmpty == false)

        let fromData = Fixture.directory.appending(path: "import-data-\(UUID().uuidString)", directoryHint: .isDirectory)
        try await LinuxRootImage.archive(data: Data(contentsOf: archive)).install(to: fromData)

        for root in [fromFile, fromData] {
            let data = root.appending(path: "data", directoryHint: .isDirectory)
            #expect(try Data(contentsOf: data.appending(path: String(marker.dropFirst()))) == Data("exported".utf8))
            #expect(try String(contentsOf: data.appending(path: "etc/alpine-release"), encoding: .utf8).hasPrefix("3."))
            #expect(FileManager.default.fileExists(atPath: root.appending(path: "meta.db").path(percentEncoded: false)))
        }
        await #expect(throws: (any Error).self) {
            try await LinuxRootImage.alpine.install(to: fromData)
        }
        await #expect(throws: LinuxRootImageError.self) {
            try await LinuxRootImage.archive(data: Data("not an archive".utf8))
                .install(to: Fixture.directory.appending(path: "invalid-\(UUID().uuidString)"))
        }
    }
}

private final class ProgressRecorder: @unchecked Sendable {
    private let lock = NSLock()
    private var recorded: [Double] = []

    var values: [Double] {
        lock.withLock { recorded }
    }

    func record(_ value: Double) {
        lock.withLock { recorded.append(value) }
    }
}
