import Foundation
import SwiptermAlpineRootfs
import SwiptermKit
import Testing

private final class Console: @unchecked Sendable {
    private let lock = NSLock()
    private var data = Data()
    private var halted = false

    var text: String {
        lock.withLock { String(decoding: data, as: UTF8.self) }
    }

    var hasHalted: Bool {
        lock.withLock { halted }
    }

    func append(_ chunk: Data) {
        lock.withLock { data.append(chunk) }
    }

    func halt() {
        lock.withLock { halted = true }
    }
}

private func eventually(_ description: String, _ condition: () async throws -> Bool) async throws {
    for _ in 0..<300 {
        if try await condition() {
            return
        }
        try await Task.sleep(for: .milliseconds(100))
    }
    Issue.record("timed out waiting for \(description)")
}

@Test func guestInitRunsAsProcessOneAndHalts() async throws {
    let directory = URL.temporaryDirectory.appending(path: "SwiptermKitProcessModeTests-\(UUID().uuidString)", directoryHint: .isDirectory)
    let console = Console()
    let script = "swipterm-prepared; trap 'echo trapped-term' TERM; echo console-ready; while [ ! -e /tmp/halt ]; do sleep 0.1; done; echo halting"
    let runtime = try await LinuxRuntime.boot(LinuxRuntime.Configuration(
        root: directory.appending(path: "root", directoryHint: .isDirectory),
        initialImage: .alpine,
        initMode: .process(executablePath: "/bin/sh", arguments: ["-c", script], consoleOutput: { console.append($0) }),
        autoSyncDNS: false,
        haltHandler: { console.halt() }
    )) { runtime in
        try await runtime.registerCommand(named: "swipterm-prepared") { invocation in
            try await invocation.standardOutput.write(Data("prepared\n".utf8))
            return 0
        }
    }
    try await eventually("console output") { console.text.contains("console-ready\r\n") }
    #expect(console.text.hasPrefix("prepared\r\n"))

    let output = try await runtime.run(LinuxProcess.Configuration(
        executablePath: "/bin/sh",
        arguments: ["-c", "cat /proc/1/cmdline | tr '\\0' ' '; echo; kill -9 1; kill -TERM 1; sleep 0.5; kill -0 1 && echo alive"]
    ))
    let text = String(decoding: output.standardOutput, as: UTF8.self)
    #expect(text.hasPrefix("/bin/sh -c swipterm-prepared"))
    #expect(text.hasSuffix("alive\n"))
    try await eventually("trapped SIGTERM") { console.text.contains("trapped-term\r\n") }

    try runtime.syncDNS()
    #expect(try await runtime.fileSystem.contents(of: "/etc/resolv.conf").isEmpty == false)

    try await runtime.fileSystem.writeFile("/tmp/halt", contents: Data())
    try await eventually("halt") { console.hasHalted }
    #expect(console.text.hasSuffix("halting\r\n"))
    await #expect(throws: LinuxError.ESRCH) {
        try await runtime.spawn(LinuxProcess.Configuration(executablePath: "/bin/true"))
    }
}
