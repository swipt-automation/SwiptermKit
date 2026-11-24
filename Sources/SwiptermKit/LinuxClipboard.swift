import Foundation

public struct LinuxClipboardItem: Sendable {
    public var mime: String
    public var data: Data

    public init(mime: String, data: Data) {
        self.mime = mime
        self.data = data
    }
}

public protocol LinuxClipboard: Sendable {
    func read(mime: String) throws -> Data?
    func write(_ items: [LinuxClipboardItem])
}
