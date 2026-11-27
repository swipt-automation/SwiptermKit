import Foundation

public struct LinuxClipboardItem: Sendable {
    public let mime: String
    public let data: Data

    public init(mime: String, data: Data) {
        self.mime = mime
        self.data = data
    }
}

public protocol LinuxClipboard: Sendable {
    func read(mime: String) throws -> Data?
    func write(_ items: [LinuxClipboardItem])
}
