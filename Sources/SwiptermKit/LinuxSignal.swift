public struct LinuxSignal: RawRepresentable, Hashable, Sendable {
    public let rawValue: Int32

    public init(rawValue: Int32) {
        self.rawValue = rawValue
    }

    public static let hangup = LinuxSignal(rawValue: 1)
    public static let interrupt = LinuxSignal(rawValue: 2)
    public static let quit = LinuxSignal(rawValue: 3)
    public static let abort = LinuxSignal(rawValue: 6)
    public static let kill = LinuxSignal(rawValue: 9)
    public static let user1 = LinuxSignal(rawValue: 10)
    public static let segmentationFault = LinuxSignal(rawValue: 11)
    public static let user2 = LinuxSignal(rawValue: 12)
    public static let pipe = LinuxSignal(rawValue: 13)
    public static let alarm = LinuxSignal(rawValue: 14)
    public static let term = LinuxSignal(rawValue: 15)
    public static let child = LinuxSignal(rawValue: 17)
    public static let cont = LinuxSignal(rawValue: 18)
    public static let stop = LinuxSignal(rawValue: 19)
    public static let terminalStop = LinuxSignal(rawValue: 20)
    public static let winch = LinuxSignal(rawValue: 28)
}
