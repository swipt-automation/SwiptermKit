import Foundation

public enum LinuxFileAccess: Sendable {
    case readOnly
    case writeOnly
    case readWrite
}

public protocol LinuxCharacterDevice: Sendable {
    func open(access: LinuxFileAccess) throws -> any LinuxCharacterDeviceFile
}

/// Serves one open file description of a device. The methods run synchronously on the thread of the
/// guest process making the system call, which waits for them and cannot be interrupted by signals
/// meanwhile. An empty result from `read` is end of file; a thrown `LinuxError` becomes the errno of
/// the system call, and any other error becomes `EIO`.
public protocol LinuxCharacterDeviceFile: AnyObject, Sendable {
    func read(maxLength: Int) throws -> Data
    func write(_ data: Data) throws -> Int
    func close()
}

extension LinuxCharacterDeviceFile {
    public func read(maxLength: Int) throws -> Data {
        throw LinuxError.EINVAL
    }

    public func write(_ data: Data) throws -> Int {
        throw LinuxError.EINVAL
    }

    public func close() {}
}

final class DeviceBox: Sendable {
    let device: any LinuxCharacterDevice

    init(device: any LinuxCharacterDevice) {
        self.device = device
    }
}

final class DeviceFileBox {
    let file: any LinuxCharacterDeviceFile

    init(file: any LinuxCharacterDeviceFile) {
        self.file = file
    }
}
