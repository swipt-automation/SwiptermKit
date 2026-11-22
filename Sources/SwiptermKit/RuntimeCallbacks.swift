import CSwiptermKit
import Foundation

final class RuntimeCallbacks: Sendable {
    let queue = DispatchQueue(label: "SwiptermKit.Callbacks")
    let consoleOutput: (@Sendable (Data) -> Void)?
    let haltHandler: (@Sendable (LinuxRuntime.HaltReason) -> Void)?
    let panicHandler: (@Sendable (String) -> Void)?

    init(
        consoleOutput: (@Sendable (Data) -> Void)?,
        haltHandler: (@Sendable (LinuxRuntime.HaltReason) -> Void)?,
        panicHandler: (@Sendable (String) -> Void)?
    ) {
        self.consoleOutput = consoleOutput
        self.haltHandler = haltHandler
        self.panicHandler = panicHandler
    }

    func makeTable(includeConsoleOutput: Bool, includePanic: Bool) -> swipterm_callbacks {
        swipterm_callbacks(
            context: Unmanaged.passUnretained(self).toOpaque(),
            console_output: includeConsoleOutput ? { context, data, length in
                let callbacks = Unmanaged<RuntimeCallbacks>.fromOpaque(context!).takeUnretainedValue()
                let chunk = Data(bytes: data!, count: length)
                callbacks.queue.async {
                    callbacks.consoleOutput?(chunk)
                }
            } : nil,
            halt: { context, reason, waitStatus in
                let callbacks = Unmanaged<RuntimeCallbacks>.fromOpaque(context!).takeUnretainedValue()
                let haltReason = LinuxRuntime.HaltReason(reason, waitStatus: waitStatus)
                callbacks.queue.async {
                    callbacks.haltHandler?(haltReason)
                }
            },
            panic: includePanic ? { context, message in
                let callbacks = Unmanaged<RuntimeCallbacks>.fromOpaque(context!).takeUnretainedValue()
                let text = String(cString: message!)
                callbacks.queue.async {
                    callbacks.panicHandler?(text)
                }
            } : nil,
            process_exit: { context, status in
                Unmanaged<LinuxProcess.State>.fromOpaque(context!).takeRetainedValue().exit(status: status)
            },
            command_start: { context, invocation, argc, argv, envp, workingDirectory, input, output, error in
                let command = Unmanaged<CommandBox>.fromOpaque(context!).takeUnretainedValue()
                let arguments = (0..<argc).map { String(cString: argv![$0]!) }
                var environment: [String: String] = [:]
                var entry = envp!
                while let pointer = entry.pointee {
                    let string = String(cString: pointer)
                    if let separator = string.firstIndex(of: "="), environment[String(string[..<separator])] == nil {
                        environment[String(string[..<separator])] = String(string[string.index(after: separator)...])
                    }
                    entry += 1
                }
                let box = command.start(
                    invocation!,
                    arguments: arguments,
                    environment: environment,
                    workingDirectory: String(cString: workingDirectory!),
                    descriptors: (input, output, error)
                )
                return Unmanaged.passRetained(box).toOpaque()
            },
            command_abort: { context in
                Unmanaged<InvocationBox>.fromOpaque(context!).takeUnretainedValue().task.cancel()
            },
            command_release: { context in
                Unmanaged<InvocationBox>.fromOpaque(context!).release()
            },
            device_open: { context, flags, errorOut in
                let device = Unmanaged<DeviceBox>.fromOpaque(context!).takeUnretainedValue().device
                let access: LinuxFileAccess = switch flags & 3 {
                case 1: .writeOnly
                case 2: .readWrite
                default: .readOnly
                }
                do {
                    let file = try device.open(access: access)
                    return Unmanaged.passRetained(DeviceFileBox(file: file)).toOpaque()
                } catch {
                    errorOut!.pointee = -((error as? LinuxError) ?? .EIO).rawValue
                    return nil
                }
            },
            device_read: { file, buffer, length in
                do {
                    let data = try Unmanaged<DeviceFileBox>.fromOpaque(file!).takeUnretainedValue().file.read(maxLength: length)
                    let count = min(data.count, length)
                    data.copyBytes(to: buffer!.assumingMemoryBound(to: UInt8.self), count: count)
                    return count
                } catch {
                    return -Int(((error as? LinuxError) ?? .EIO).rawValue)
                }
            },
            device_write: { file, buffer, length in
                do {
                    let data = Data(bytes: buffer!, count: length)
                    return try Unmanaged<DeviceFileBox>.fromOpaque(file!).takeUnretainedValue().file.write(data)
                } catch {
                    return -Int(((error as? LinuxError) ?? .EIO).rawValue)
                }
            },
            device_close: { file in
                Unmanaged<DeviceFileBox>.fromOpaque(file!).takeRetainedValue().file.close()
            }
        )
    }
}
