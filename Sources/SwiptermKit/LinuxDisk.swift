import CSwiptermKit
import Foundation

/// The file system of a root image attached to a running machine, beside the root the machine booted.
/// The image stays locked against booting, building and exporting until `detach()` returns or the machine shuts down.
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

    /// Makes the file system appear at `guestPath` in the machine's container; an existing directory there must be empty.
    public func mount(at guestPath: String) async throws {
        let id = id
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_disk_mount(runtimeHandle, id, guestPath))
        }
    }

    /// Throws `LinuxError.EBUSY` while the container uses files below `guestPath`.
    public func unmount(at guestPath: String) async throws {
        let id = id
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_disk_unmount(runtimeHandle, id, guestPath))
        }
    }

    /// Unmounts the file system everywhere and releases the image. Throws `LinuxError.EBUSY` while a process started
    /// by `spawn` or `shell` runs or the container uses files below a mount point, leaving the disk attached.
    public func detach() async throws {
        let id = id
        try await runtime.withActiveHandleBlocking { runtimeHandle in
            try check(swipterm_disk_detach(runtimeHandle, id))
        }
    }

    /// Runs a process as root with the disk as its root directory, in PID, mount, UTS and IPC namespaces of its own
    /// that share the container's network. `/proc`, `/sys`, `/dev`, `/run` and `/tmp` are mounted for it, name
    /// resolution is configured, `container=lxc` is set, and packages installed in it cannot start services.
    /// When the process exits, every process it left behind is killed and its mounts are gone.
    /// An `executablePath` without a slash is searched in the `PATH` of the environment.
    /// With `invocation`, the process uses the standard streams of that host command instead of the configured
    /// ones; a terminal there keeps its job control and window size.
    public func spawn(
        _ configuration: LinuxProcess.Configuration,
        hostname: String? = nil,
        attachedTo invocation: HostCommandInvocation? = nil
    ) async throws -> LinuxProcess {
        try await launch(
            configuration,
            environment: configuration.environment ?? runtime.defaultEnvironment,
            target: LinuxProcess.Target(disk: id, hostname: hostname, shell: false, invocation: invocation?.handle),
            invocation: invocation
        )
    }

    /// Runs the login shell of `user` like `spawn`, falling back to `/bin/sh` for root when the disk has no account
    /// database. The shell keeps `TERM`, `COLORTERM`, `LANG`, `LANGUAGE`, `LC_*`, `TZ` and `BROWSER` from `environment`.
    public func shell(
        user: String = "root",
        environment: [String: String] = [:],
        hostname: String? = nil,
        attachedTo invocation: HostCommandInvocation
    ) async throws -> LinuxProcess {
        try await launch(
            LinuxProcess.Configuration(executablePath: user),
            environment: environment,
            target: LinuxProcess.Target(disk: id, hostname: hostname, shell: true, invocation: invocation.handle),
            invocation: invocation
        )
    }

    private func launch(
        _ configuration: LinuxProcess.Configuration,
        environment: [String: String],
        target: LinuxProcess.Target,
        invocation: HostCommandInvocation?
    ) async throws -> LinuxProcess {
        let process = try await LinuxProcess(runtime: runtime, configuration, environment: environment, target: target)
        if let invocation, let input = process.standardInput {
            Task {
                while let chunk = try? await invocation.standardInput.read() {
                    guard (try? await input.write(chunk)) != nil else { break }
                }
                input.close()
            }
        }
        return process
    }
}
