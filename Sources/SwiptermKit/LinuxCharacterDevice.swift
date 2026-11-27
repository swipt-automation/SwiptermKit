import Foundation

public enum LinuxFileAccess: Sendable {
    case readOnly
    case writeOnly
    case readWrite
}

public protocol LinuxCharacterDevice: Sendable {
    func open(access: LinuxFileAccess) throws -> any LinuxCharacterDeviceFile
}

public protocol LinuxCharacterDeviceFile: Sendable {
    func read(maxLength: Int) throws -> Data
    func write(_ data: Data) throws -> Int
    func close()
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
