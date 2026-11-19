import CSwiptermKit
import Foundation

final class RuntimeCallbacks: Sendable {
    let consoleOutput: (@Sendable (Data) -> Void)?
    let haltHandler: (@Sendable () -> Void)?
    let panicHandler: (@Sendable (String) -> Void)?

    init(
        consoleOutput: (@Sendable (Data) -> Void)?,
        haltHandler: (@Sendable () -> Void)?,
        panicHandler: (@Sendable (String) -> Void)?
    ) {
        self.consoleOutput = consoleOutput
        self.haltHandler = haltHandler
        self.panicHandler = panicHandler
    }

    nonisolated(unsafe) private static var installed: RuntimeCallbacks?
    nonisolated(unsafe) private static var booting = false
    private static let lock = NSLock()

    static func boot(_ callbacks: RuntimeCallbacks, _ body: () async throws -> Void) async throws {
        try lock.withLock {
            guard !booting else {
                throw LinuxError.EBUSY
            }
            booting = true
            installed = callbacks
        }
        do {
            try await body()
        } catch {
            lock.withLock {
                booting = false
            }
            throw error
        }
    }

    static let table = swipterm_callbacks(
        console_output: { data, length in
            installed?.consoleOutput?(Data(bytes: data!, count: length))
        },
        halt: {
            installed?.haltHandler?()
        },
        panic: { message in
            installed?.panicHandler?(String(cString: message!))
        },
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
            Unmanaged<InvocationBox>.fromOpaque(context!).takeUnretainedValue().cancel()
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
