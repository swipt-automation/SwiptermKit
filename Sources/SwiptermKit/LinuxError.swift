import Foundation

public struct LinuxError: RawRepresentable, Hashable, Sendable, LocalizedError {
    public let rawValue: Int32

    public init(rawValue: Int32) {
        self.rawValue = rawValue
    }

    init(code: Int32) {
        rawValue = -code
    }

    public static let EPERM = LinuxError(rawValue: 1)
    public static let ENOENT = LinuxError(rawValue: 2)
    public static let ESRCH = LinuxError(rawValue: 3)
    public static let EINTR = LinuxError(rawValue: 4)
    public static let EIO = LinuxError(rawValue: 5)
    public static let ENXIO = LinuxError(rawValue: 6)
    public static let E2BIG = LinuxError(rawValue: 7)
    public static let ENOEXEC = LinuxError(rawValue: 8)
    public static let EBADF = LinuxError(rawValue: 9)
    public static let ECHILD = LinuxError(rawValue: 10)
    public static let EAGAIN = LinuxError(rawValue: 11)
    public static let ENOMEM = LinuxError(rawValue: 12)
    public static let EACCES = LinuxError(rawValue: 13)
    public static let EFAULT = LinuxError(rawValue: 14)
    public static let EBUSY = LinuxError(rawValue: 16)
    public static let EEXIST = LinuxError(rawValue: 17)
    public static let EXDEV = LinuxError(rawValue: 18)
    public static let ENODEV = LinuxError(rawValue: 19)
    public static let ENOTDIR = LinuxError(rawValue: 20)
    public static let EISDIR = LinuxError(rawValue: 21)
    public static let EINVAL = LinuxError(rawValue: 22)
    public static let ENFILE = LinuxError(rawValue: 23)
    public static let EMFILE = LinuxError(rawValue: 24)
    public static let ENOTTY = LinuxError(rawValue: 25)
    public static let EFBIG = LinuxError(rawValue: 27)
    public static let ENOSPC = LinuxError(rawValue: 28)
    public static let ESPIPE = LinuxError(rawValue: 29)
    public static let EROFS = LinuxError(rawValue: 30)
    public static let EMLINK = LinuxError(rawValue: 31)
    public static let EPIPE = LinuxError(rawValue: 32)
    public static let ERANGE = LinuxError(rawValue: 34)
    public static let ENAMETOOLONG = LinuxError(rawValue: 36)
    public static let ENOSYS = LinuxError(rawValue: 38)
    public static let ENOTEMPTY = LinuxError(rawValue: 39)
    public static let ELOOP = LinuxError(rawValue: 40)
    public static let ETIMEDOUT = LinuxError(rawValue: 110)

    private static let descriptions: [Int32: String] = [
        1: "Operation not permitted",
        2: "No such file or directory",
        3: "No such process",
        4: "Interrupted system call",
        5: "I/O error",
        6: "No such device or address",
        7: "Argument list too long",
        8: "Exec format error",
        9: "Bad file descriptor",
        10: "No child processes",
        11: "Resource temporarily unavailable",
        12: "Out of memory",
        13: "Permission denied",
        14: "Bad address",
        16: "Device or resource busy",
        17: "File exists",
        18: "Invalid cross-device link",
        19: "No such device",
        20: "Not a directory",
        21: "Is a directory",
        22: "Invalid argument",
        23: "Too many open files in system",
        24: "Too many open files",
        25: "Inappropriate ioctl for device",
        27: "File too large",
        28: "No space left on device",
        29: "Illegal seek",
        30: "Read-only file system",
        31: "Too many links",
        32: "Broken pipe",
        34: "Numerical result out of range",
        36: "File name too long",
        38: "Function not implemented",
        39: "Directory not empty",
        40: "Too many levels of symbolic links",
        110: "Connection timed out",
    ]

    public var errorDescription: String? {
        Self.descriptions[rawValue] ?? "Linux error \(rawValue)"
    }
}

func check(_ code: Int32) throws(LinuxError) {
    if code < 0 {
        throw LinuxError(code: code)
    }
}
