import CSwiptermAlpineRootfs
import SwiptermKit

extension LinuxRootImage {
    public static var alpine: LinuxRootImage {
        .daxMemory(UnsafeRawBufferPointer(start: swipterm_alpine_rootfs_data, count: swipterm_alpine_rootfs_size))
    }
}
