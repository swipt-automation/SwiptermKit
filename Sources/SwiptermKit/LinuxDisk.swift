import CSwiptermKit
import Foundation

public final class LinuxDisk: Sendable {
    public let isReadOnly: Bool

    private let runtime: LinuxRuntime
    private let id: UInt32

    init(runtime: LinuxRuntime, root: URL, readOnly: Bool) async throws {
        let rootPath = root.path(percentEncoded: false)
        id = try await runtime.withActiveHandleBlocking { runtimeHandle in
            var id: UInt32 = 0
            try check(swipterm_disk_attach(runtimeHandle, rootPath, readOnly, &id))
            return id
        }
        self.runtime = runtime
        isReadOnly = readOnly
    }

    public func mount(at guestPath: String) async throws {
        let id = id
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_disk_mount(runtimeHandle, id, guestPath))
        }
    }

    public func unmount(at guestPath: String) async throws {
        let id = id
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_disk_unmount(runtimeHandle, id, guestPath))
        }
    }

    public func detach() async throws {
        let id = id
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_disk_detach(runtimeHandle, id))
        }
    }

    public func spawn(
        _ configuration: LinuxProcess.Configuration,
        hostname: String,
        attachedTo invocation: HostCommandInvocation? = nil
    ) async throws -> LinuxProcess {
        try await LinuxProcess(runtime: runtime, disk: id, configuration, hostname: hostname, shell: false, invocation: invocation)
    }

    public func shell(
        environment: [String: String],
        hostname: String,
        attachedTo invocation: HostCommandInvocation
    ) async throws -> LinuxProcess {
        try await LinuxProcess(
            runtime: runtime,
            disk: id,
            LinuxProcess.Configuration(executablePath: "root", environment: environment),
            hostname: hostname,
            shell: true,
            invocation: invocation
        )
    }
}
