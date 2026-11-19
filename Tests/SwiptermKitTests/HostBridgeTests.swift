import Foundation
import SwiptermKit
import Testing

private struct InvocationRecord: Codable, Equatable {
    var arguments: [String]
    var variable: String?
    var workingDirectory: String
}

private final class MemoryDevice: LinuxCharacterDevice {
    final class Storage: @unchecked Sendable {
        private let lock = NSLock()
        private var data = Data()

        var contents: Data {
            lock.withLock { data }
        }

        func replace(with data: Data) {
            lock.withLock { self.data = data }
        }

        func append(_ data: Data) {
            lock.withLock { self.data.append(data) }
        }
    }

    final class File: LinuxCharacterDeviceFile, @unchecked Sendable {
        let storage: Storage
        var offset = 0

        init(storage: Storage) {
            self.storage = storage
        }

        func read(maxLength: Int) throws -> Data {
            let contents = storage.contents
            let chunk = contents.dropFirst(offset).prefix(maxLength)
            offset += chunk.count
            return Data(chunk)
        }

        func write(_ data: Data) throws -> Int {
            storage.append(data)
            return data.count
        }
    }

    let storage = Storage()

    func open(access: LinuxFileAccess) throws -> any LinuxCharacterDeviceFile {
        if access != .readOnly {
            storage.replace(with: Data())
        }
        return File(storage: storage)
    }
}

private struct DeniedDevice: LinuxCharacterDevice {
    func open(access: LinuxFileAccess) throws -> any LinuxCharacterDeviceFile {
        throw LinuxError.EACCES
    }
}

@Suite struct HostBridgeTests {
    @Test func commandsReceiveRawInvocation() async throws {
        let runtime = try await Fixture.runtime
        try await runtime.registerCommand(named: "swipterm-record") { invocation in
            let record = InvocationRecord(
                arguments: invocation.arguments,
                variable: invocation.environment["SWIPTERM_VARIABLE"],
                workingDirectory: invocation.workingDirectory
            )
            var header = try JSONEncoder().encode(record)
            header.append(0x0A)
            try await invocation.standardOutput.write(header)
            for try await chunk in invocation.standardInput {
                try await invocation.standardOutput.write(chunk)
            }
            try await invocation.standardError.write(Data("to stderr\r\n".utf8))
            return 7
        }
        await #expect(throws: LinuxError.EEXIST) {
            try await runtime.registerCommand(named: "swipterm-record") { _ in 0 }
        }

        let input = Data([0x00, 0x0D, 0x0A, 0xFF, 0x0D, 0x41])
        let output = try await shell(
            "cd /tmp && SWIPTERM_VARIABLE='a=b' swipterm-record '' 'two words' ./relative /abs/path 2>/tmp/record-error; echo \"status=$?\"",
            input: input
        )
        let headerEnd = try #require(output.standardOutput.firstIndex(of: 0x0A))
        let record = try JSONDecoder().decode(InvocationRecord.self, from: output.standardOutput[..<headerEnd])
        #expect(record == InvocationRecord(
            arguments: ["", "two words", "./relative", "/abs/path"],
            variable: "a=b",
            workingDirectory: "/tmp"
        ))
        #expect(output.standardOutput[(headerEnd + 1)...] == input + Data("status=7\n".utf8))
        #expect(try await runtime.fileSystem.contents(of: "/tmp/record-error") == Data("to stderr\r\n".utf8))

        let direct = try await runtime.run(LinuxProcess.Configuration(
            executablePath: "/run/host-bin/swipterm-record",
            arguments: ["direct"],
            workingDirectory: "/"
        ))
        #expect(direct.termination == .exited(7))
        #expect(direct.text.hasPrefix("{"))
        #expect(direct.errorText == "to stderr\r\n")

        let shadow = try await shell(
            "mkdir -p /tmp/shadow && printf '#!/bin/sh\\necho guest\\n' > /tmp/shadow/swipterm-record && chmod +x /tmp/shadow/swipterm-record && PATH=/tmp/shadow:$PATH swipterm-record"
        )
        #expect(shadow.text == "guest\n")
    }

    @Test func signalsCancelRunningCommands() async throws {
        let runtime = try await Fixture.runtime
        let started = Signal()
        let cancelled = Signal()
        try await runtime.registerCommand(named: "swipterm-wait") { _ in
            started.fire()
            do {
                try await Task.sleep(for: .seconds(600))
            } catch {
                cancelled.fire()
                throw error
            }
            return 0
        }

        let shellProcess = try await runtime.spawn(LinuxProcess.Configuration(
            executablePath: "/bin/sh",
            arguments: ["-c", "swipterm-wait; echo \"after=$?\""],
            standardOutput: .pipe
        ))
        try await started.wait("command start")
        #expect(try await shell("pkill -TERM -x swipterm-wait; echo $?").text == "0\n")
        try await cancelled.wait("SIGTERM cancellation")
        let after = String(decoding: try await shellProcess.standardOutput!.readToEnd(), as: UTF8.self)
        #expect(after == "after=143\n")

        let direct = try await runtime.spawn(LinuxProcess.Configuration(executablePath: "/run/host-bin/swipterm-wait"))
        try await started.wait("command start")
        try direct.send(.kill)
        try await cancelled.wait("SIGKILL cancellation")
        let termination = try await withTimeout(20, "termination") { await direct.termination }
        #expect(termination == .signaled(.kill))
    }

    @Test func devicesServeGuestReadsAndWrites() async throws {
        let runtime = try await Fixture.runtime
        let device = MemoryDevice()
        try await runtime.registerDevice(at: "/dev/swipterm-memory", handler: device)
        try await runtime.registerDevice(at: "/dev/swipterm-denied", handler: DeniedDevice())

        let output = try await shell("printf 'hello\\0device' > /dev/swipterm-memory && cat /dev/swipterm-memory && ls -l /dev/swipterm-memory")
        #expect(output.termination == .exited(0))
        #expect(output.standardOutput.starts(with: Data("hello\0device".utf8)))
        #expect(output.text.contains("crw-rw-rw-"))
        #expect(device.storage.contents == Data("hello\0device".utf8))

        let denied = try await shell("cat /dev/swipterm-denied")
        #expect(denied.termination == .exited(1))
        #expect(denied.errorText.contains("Permission denied"))
    }
}
