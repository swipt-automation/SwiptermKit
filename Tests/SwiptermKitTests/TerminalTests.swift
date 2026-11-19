import Foundation
import SwiptermKit
import Testing

private actor TerminalReader {
    let terminal: LinuxTerminal
    private(set) var text = ""
    private(set) var byteCount = 0

    init(_ terminal: LinuxTerminal) {
        self.terminal = terminal
    }

    func read(until needle: String, timeout: Double = 60) async throws -> String {
        try await withTimeout(timeout, "waiting for \(needle.debugDescription) in \(text.suffix(200).debugDescription)") {
            while await !self.text.contains(needle) {
                guard let chunk = await self.terminal.read() else {
                    throw TimeoutError(description: "terminal ended before \(needle.debugDescription)")
                }
                await self.append(chunk)
            }
            return await self.text
        }
    }

    func readToEnd(timeout: Double = 60) async throws -> String {
        try await withTimeout(timeout, "terminal end") {
            while let chunk = await self.terminal.read() {
                await self.append(chunk)
            }
            return await self.text
        }
    }

    func reset() {
        text = ""
    }

    private func append(_ chunk: Data) {
        byteCount += chunk.count
        text += String(decoding: chunk, as: UTF8.self)
    }
}

@Suite struct TerminalTests {
    private func spawnShell(on terminal: LinuxTerminal) async throws -> LinuxProcess {
        var environment = try await Fixture.runtime.defaultEnvironment
        environment["PS1"] = "PROMPT> "
        return try await Fixture.runtime.spawn(LinuxProcess.Configuration(
            executablePath: "/bin/sh",
            environment: environment,
            standardInput: .terminal(terminal),
            standardOutput: .terminal(terminal),
            standardError: .terminal(terminal)
        ))
    }

    @Test func interactiveShellEchoesResizesAndCloses() async throws {
        let runtime = try await Fixture.runtime
        let terminal = try runtime.makeTerminal(size: LinuxTerminal.Size(columns: 80, rows: 24))
        let process = try await spawnShell(on: terminal)
        let reader = TerminalReader(terminal)
        _ = try await reader.read(until: "PROMPT> ")

        await terminal.write(Data("echo hel''lo; tty\n".utf8))
        let echoed = try await reader.read(until: "hello\r\n")
        #expect(echoed.contains("echo hel''lo; tty"))
        _ = try await reader.read(until: "/dev/pts/")

        await reader.reset()
        terminal.resize(LinuxTerminal.Size(columns: 100, rows: 30))
        await terminal.write(Data("stty size\n".utf8))
        _ = try await reader.read(until: "30 100\r\n")

        await #expect(throws: LinuxError.EBUSY) {
            try await runtime.spawn(LinuxProcess.Configuration(executablePath: "/bin/true", standardOutput: .terminal(terminal)))
        }

        terminal.close()
        _ = try await withTimeout(20, "hangup") { await process.termination }
        _ = try await reader.readToEnd(timeout: 20)
        await #expect(throws: LinuxError.EIO) {
            try await runtime.spawn(LinuxProcess.Configuration(executablePath: "/bin/true", standardOutput: .terminal(terminal)))
        }
    }

    @Test func outputAppliesBackpressureWithoutBlockingEcho() async throws {
        let runtime = try await Fixture.runtime
        let terminal = try runtime.makeTerminal(size: LinuxTerminal.Size(columns: 80, rows: 24))
        _ = try await spawnShell(on: terminal)
        let reader = TerminalReader(terminal)
        _ = try await reader.read(until: "PROMPT> ")

        await terminal.write(Data("head -c 400000 /dev/zero | tr '\\0' x; echo; echo DONE-$((40+2))\n".utf8))
        try await Task.sleep(for: .seconds(1))
        let producing = try await shell("pgrep -x head >/dev/null && echo blocked").text
        #expect(producing == "blocked\n")

        try await withTimeout(20, "echo while output is full") {
            await terminal.write(Data(String(repeating: "y", count: 3000).utf8))
            await terminal.write(Data("\u{15}echo SECOND-$((40+2))\n".utf8))
        }
        _ = try await reader.read(until: "DONE-42")
        _ = try await reader.read(until: "SECOND-42")
        #expect(await reader.byteCount > 400000)
        terminal.close()
    }

    @Test func outputEndsWhenTheLastProcessExits() async throws {
        let runtime = try await Fixture.runtime
        let terminal = try runtime.makeTerminal(size: LinuxTerminal.Size(columns: 80, rows: 24))
        let process = try await runtime.spawn(LinuxProcess.Configuration(
            executablePath: "/bin/sh",
            arguments: ["-c", "tty; echo bye"],
            standardInput: .terminal(terminal),
            standardOutput: .terminal(terminal),
            standardError: .terminal(terminal)
        ))
        let text = try await TerminalReader(terminal).readToEnd(timeout: 30)
        #expect(text.hasSuffix("bye\r\n"))
        #expect(text.hasPrefix("/dev/pts/"))
        #expect(await process.termination == .exited(0))
    }
}
