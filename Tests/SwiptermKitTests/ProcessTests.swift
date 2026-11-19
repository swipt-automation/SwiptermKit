import Foundation
import SwiptermKit
import Testing

@Suite struct ProcessTests {
    @Test func bootsAlpineInManagedMode() async throws {
        let output = try await shell("cat /etc/alpine-release; uname -n; cat /proc/ish/version; echo \"$PATH\"")
        #expect(output.termination == .exited(0))
        let lines = output.text.split(separator: "\n").map(String.init)
        #expect(lines.first?.hasPrefix("3.") == true)
        #expect(lines.dropFirst().first == "swipterm-tests")
        #expect(lines.dropFirst(2).first == "SwiptermKitTests")
        #expect(lines.last?.split(separator: ":").contains("/run/host-bin") == true)
    }

    @Test func killingInitFromGuestKeepsItAlive() async throws {
        let output = try await shell("kill -9 1; kill -TERM 1; kill -INT 1; sleep 0.2; kill -0 1 && echo alive")
        #expect(output.text == "alive\n")
        #expect(try await shell("echo still running").text == "still running\n")
    }

    @Test func policyEnvironmentIsNotInjected() async throws {
        for output in [
            try await Fixture.runtime.run(LinuxProcess.Configuration(executablePath: "/usr/bin/env")),
            try await shell("exec env"),
        ] {
            let variables = Set(output.text.split(separator: "\n").map { String($0.split(separator: "=")[0]) })
            #expect(!variables.contains("NO_COLOR"))
            #expect(!variables.contains("PIP_PROGRESS_BAR"))
            #expect(!variables.contains("PYTHONDONTWRITEBYTECODE"))
            #expect(variables.contains("GOMAXPROCS"))
            #expect(variables.contains("PYTHONMALLOC"))
        }
    }

    @Test func runCapturesOutputErrorAndStatus() async throws {
        let output = try await shell("echo out; echo err >&2; exit 3")
        #expect(output.termination == .exited(3))
        #expect(output.text == "out\n")
        #expect(output.errorText == "err\n")
    }

    @Test func pipesCarryBinaryData() async throws {
        let input = Data((0..<(1 << 20)).map { UInt8(truncatingIfNeeded: $0 &* 31 &+ $0 >> 8) })
        let output = try await Fixture.runtime.run(LinuxProcess.Configuration(executablePath: "/bin/cat"), input: input)
        #expect(output.termination == .exited(0))
        #expect(output.standardOutput == input)
        #expect(output.standardError.isEmpty)
    }

    @Test func spawnedPipesDeliverEndOfFile() async throws {
        let process = try await Fixture.runtime.spawn(LinuxProcess.Configuration(
            executablePath: "/usr/bin/wc",
            arguments: ["-c"],
            standardInput: .pipe,
            standardOutput: .pipe
        ))
        let input = try #require(process.standardInput)
        try await input.write(Data(repeating: 0x0D, count: 12345))
        input.close()
        let output = try await process.standardOutput!.readToEnd()
        #expect(String(decoding: output, as: UTF8.self).trimmingCharacters(in: .whitespacesAndNewlines) == "12345")
        #expect(await process.termination == .exited(0))
    }

    @Test func guestPollsPipedStandardInput() async throws {
        let process = try await Fixture.runtime.spawn(LinuxProcess.Configuration(
            executablePath: "/bin/sh",
            arguments: ["-c", "read -t 1 early || echo timeout; read -t 20 line && echo \"got:$line\""],
            standardInput: .pipe,
            standardOutput: .pipe
        ))
        let output = process.standardOutput!
        var text = ""
        while !text.contains("timeout") {
            text += String(decoding: try #require(try await output.read()), as: UTF8.self)
        }
        try await process.standardInput!.write(Data("hello\n".utf8))
        text += String(decoding: try await output.readToEnd(), as: UTF8.self)
        #expect(text == "timeout\ngot:hello\n")
        #expect(await process.termination == .exited(0))
    }

    @Test func workingDirectoryAndMissingExecutable() async throws {
        #expect(try await shell("pwd", workingDirectory: "/tmp").text == "/tmp\n")
        await #expect(throws: LinuxError.ENOENT) {
            try await Fixture.runtime.spawn(LinuxProcess.Configuration(executablePath: "/no/such/program"))
        }
        await #expect(throws: LinuxError.ENOENT) {
            try await Fixture.runtime.spawn(LinuxProcess.Configuration(executablePath: "/bin/sh", workingDirectory: "/no/such/directory"))
        }
    }

    @Test func signalsInterruptBlockedReads() async throws {
        let process = try await Fixture.runtime.spawn(LinuxProcess.Configuration(
            executablePath: "/bin/cat",
            standardInput: .pipe,
            standardOutput: .pipe
        ))
        try await Task.sleep(for: .milliseconds(300))
        try process.send(.interrupt)
        let termination = try await withTimeout(20, "SIGINT") { await process.termination }
        #expect(termination == .signaled(.interrupt))
        #expect(throws: LinuxError.ESRCH) {
            try process.send(.kill)
        }
    }

    @Test func signalsReachTheProcessGroup() async throws {
        let process = try await Fixture.runtime.spawn(LinuxProcess.Configuration(
            executablePath: "/bin/sh",
            arguments: ["-c", "sleep 4321 & sleep 4321; wait"]
        ))
        try await Task.sleep(for: .milliseconds(500))
        let running = try await shell("pgrep -f '^sleep 432[1]' | wc -l; ps -o pid,pgid,args")
        #expect(running.text.hasPrefix("2\n"))
        try process.send(.kill, toProcessGroup: true)
        let termination = try await withTimeout(20, "SIGKILL") { await process.termination }
        #expect(termination == .signaled(.kill))
        var remaining = "2"
        for _ in 0..<50 where remaining != "0" {
            try await Task.sleep(for: .milliseconds(100))
            remaining = try await shell("pgrep -f '^sleep 432[1]' | wc -l").text.trimmingCharacters(in: .whitespacesAndNewlines)
        }
        #expect(remaining == "0")
    }
}
