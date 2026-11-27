public struct LinuxSignal: RawRepresentable, Hashable, Sendable {
    public let rawValue: Int32

    public init(rawValue: Int32) {
        self.rawValue = rawValue
    }

    public static let kill = LinuxSignal(rawValue: 9)
}
