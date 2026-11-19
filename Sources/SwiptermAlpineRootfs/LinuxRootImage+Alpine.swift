import CSwiptermAlpineRootfs
import Foundation
import SwiptermKit

extension LinuxRootImage {
    public static var alpine: LinuxRootImage {
        .archive(data: Data(
            bytesNoCopy: UnsafeMutableRawPointer(mutating: swipterm_alpine_rootfs_data),
            count: swipterm_alpine_rootfs_size,
            deallocator: .none
        ))
    }
}
