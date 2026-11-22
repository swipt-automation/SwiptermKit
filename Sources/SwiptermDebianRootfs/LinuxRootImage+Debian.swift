import CSwiptermDebianRootfs
import SwiptermKit

extension LinuxRootImage {
    public static var debian: LinuxRootImage {
        .daxMemory(UnsafeRawBufferPointer(start: swipterm_debian_rootfs_data, count: swipterm_debian_rootfs_size))
    }
}
