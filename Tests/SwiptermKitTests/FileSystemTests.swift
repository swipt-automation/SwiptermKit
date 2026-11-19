import Foundation
import SwiptermKit
import Testing

@Suite struct FileSystemTests {
    @Test func createsReadsListsAndRemovesFiles() async throws {
        let fileSystem = try await Fixture.runtime.fileSystem
        let directory = "/tmp/filesystem-\(UUID().uuidString)"
        let contents = Data((0..<70000).map { UInt8(truncatingIfNeeded: $0) })

        try await fileSystem.writeFile("\(directory)/nested/data.bin", contents: contents, permissions: 0o600)
        #expect(try await fileSystem.contents(of: "\(directory)/nested/data.bin") == contents)
        let stat = try await fileSystem.stat("\(directory)/nested/data.bin")
        #expect(stat.type == .regular)
        #expect(stat.permissions == 0o600)
        #expect(stat.size == UInt64(contents.count))
        #expect(try await shell("stat -c '%a %s' \(directory)/nested/data.bin").text == "600 70000\n")

        try await fileSystem.writeFile("\(directory)/nested/data.bin", contents: Data("replaced".utf8))
        #expect(try await fileSystem.contents(of: "\(directory)/nested/data.bin") == Data("replaced".utf8))
        #expect(try await fileSystem.stat("\(directory)/nested/data.bin").permissions == 0o644)

        try await fileSystem.createDirectory("\(directory)/a/b/c", permissions: 0o700, withIntermediateDirectories: true)
        try await fileSystem.createDirectory("\(directory)/a/b/c", withIntermediateDirectories: true)
        await #expect(throws: LinuxError.EEXIST) {
            try await fileSystem.createDirectory("\(directory)/a/b/c")
        }
        #expect(try await fileSystem.stat("\(directory)/a/b/c").permissions == 0o700)
        _ = try await shell("ln -s nested/data.bin \(directory)/link")

        let entries = try await fileSystem.listDirectory(directory).sorted { $0.name < $1.name }
        #expect(entries == [
            LinuxDirectoryEntry(name: "a", type: .directory),
            LinuxDirectoryEntry(name: "link", type: .symbolicLink),
            LinuxDirectoryEntry(name: "nested", type: .directory),
        ])
        #expect(try await fileSystem.stat("\(directory)/link", followSymlinks: false).type == .symbolicLink)
        #expect(try await fileSystem.stat("\(directory)/link").type == .regular)

        await #expect(throws: LinuxError.ENOTEMPTY) {
            try await fileSystem.removeItem("\(directory)/a")
        }
        try await fileSystem.removeItem("\(directory)/link")
        #expect(try await fileSystem.stat("\(directory)/nested/data.bin").type == .regular)
        try await fileSystem.removeItem(directory, recursive: true)
        await #expect(throws: LinuxError.ENOENT) {
            try await fileSystem.stat(directory)
        }
        await #expect(throws: LinuxError.EISDIR) {
            try await fileSystem.contents(of: "/tmp")
        }
    }

    @Test func mountsHostDirectories() async throws {
        let runtime = try await Fixture.runtime
        let host = Fixture.directory.appending(path: "mount-\(UUID().uuidString)", directoryHint: .isDirectory)
        try FileManager.default.createDirectory(at: host, withIntermediateDirectories: true)
        try Data("from host".utf8).write(to: host.appending(path: "hello.txt"))
        let writable = "/mnt/rw-\(UUID().uuidString.prefix(8))"
        let readOnly = "/mnt/ro-\(UUID().uuidString.prefix(8))"

        try await runtime.mount(host, at: writable)
        try await runtime.mount(host, at: readOnly, readOnly: true)
        #expect(try await shell("cat \(writable)/hello.txt").text == "from host")
        #expect(try await shell("echo from guest > \(writable)/guest.txt").termination == .exited(0))
        #expect(try String(contentsOf: host.appending(path: "guest.txt"), encoding: .utf8) == "from guest\n")
        #expect(try await runtime.fileSystem.contents(of: "\(readOnly)/guest.txt") == Data("from guest\n".utf8))

        let rejected = try await shell("echo nope > \(readOnly)/nope.txt")
        #expect(rejected.termination != .exited(0))
        #expect(rejected.errorText.contains("Read-only file system"))
        await #expect(throws: LinuxError.EROFS) {
            try await runtime.fileSystem.writeFile("\(readOnly)/nope.txt", contents: Data())
        }
        #expect(!FileManager.default.fileExists(atPath: host.appending(path: "nope.txt").path(percentEncoded: false)))
        await #expect(throws: LinuxError.EBUSY) {
            try await runtime.fileSystem.removeItem("/mnt", recursive: true)
        }

        try await runtime.unmount(at: writable)
        try await runtime.unmount(at: readOnly)
        #expect(try await runtime.fileSystem.listDirectory(writable).isEmpty)
        #expect(try await runtime.fileSystem.stat(readOnly).type == .directory)
        #expect(FileManager.default.fileExists(atPath: host.appending(path: "hello.txt").path(percentEncoded: false)))
        await #expect(throws: LinuxError.EINVAL) {
            try await runtime.unmount(at: writable)
        }

        try await runtime.fileSystem.writeFile("\(writable)/occupied", contents: Data())
        await #expect(throws: LinuxError.ENOTEMPTY) {
            try await runtime.mount(host, at: writable)
        }
    }

    @Test func guestCannotMountHostFileSystem() async throws {
        let output = try await shell("mkdir -p /mnt/real && mount -t real / /mnt/real")
        #expect(output.termination != .exited(0))
        #expect(output.errorText.contains("permission denied"))
        #expect(try await Fixture.runtime.fileSystem.listDirectory("/mnt/real").isEmpty)
    }
}
