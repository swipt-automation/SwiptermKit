import CSwiptermKit
import Foundation

public struct HostCommandInvocation: Sendable {
    public let arguments: [String]
    public let environment: [String: String]
    public let workingDirectory: String
    public let standardInput: LinuxPipeReader
    public let standardOutput: LinuxPipeWriter
    public let standardError: LinuxPipeWriter
}

final class CommandBox: Sendable {
    let name: String
    let handler: @Sendable (HostCommandInvocation) async throws -> Int32

    init(name: String, handler: @escaping @Sendable (HostCommandInvocation) async throws -> Int32) {
        self.name = name
        self.handler = handler
    }

    func start(
        _ invocation: OpaquePointer,
        arguments: [String],
        environment: [String: String],
        workingDirectory: String,
        descriptors: (Int32, Int32, Int32)
    ) -> InvocationBox {
        let context = HostCommandInvocation(
            arguments: arguments,
            environment: environment,
            workingDirectory: workingDirectory,
            standardInput: LinuxPipeReader(fileDescriptor: descriptors.0),
            standardOutput: LinuxPipeWriter(fileDescriptor: descriptors.1),
            standardError: LinuxPipeWriter(fileDescriptor: descriptors.2)
        )
        let handle = UnsafeSendable(value: invocation)
        let task = Task { [name, handler] in
            var status: Int32
            do {
                status = try await handler(context)
            } catch is CancellationError {
                status = 1
            } catch {
                status = 1
                try? await context.standardError.write(Data("\(name): \(error.localizedDescription)\n".utf8))
            }
            context.standardInput.close()
            context.standardOutput.close()
            context.standardError.close()
            swipterm_invocation_finish(handle.value, status)
            swipterm_invocation_release(handle.value)
        }
        return InvocationBox(task: task)
    }
}

final class InvocationBox: Sendable {
    let task: Task<Void, Never>

    init(task: Task<Void, Never>) {
        self.task = task
    }
}
