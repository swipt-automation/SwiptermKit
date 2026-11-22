#ifndef VMM_HOST_INTERNAL_H
#define VMM_HOST_INTERNAL_H

#include "../../Guest/swiptermd/protocol.h"
#include "../virtio/virtio_mmio.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    LINUX_EPERM = 1,
    LINUX_ENOENT = 2,
    LINUX_ESRCH = 3,
    LINUX_EINTR = 4,
    LINUX_EIO = 5,
    LINUX_ENXIO = 6,
    LINUX_EBADF = 9,
    LINUX_EAGAIN = 11,
    LINUX_ENOMEM = 12,
    LINUX_EACCES = 13,
    LINUX_EBUSY = 16,
    LINUX_EEXIST = 17,
    LINUX_ENODEV = 19,
    LINUX_ENOTDIR = 20,
    LINUX_EISDIR = 21,
    LINUX_EINVAL = 22,
    LINUX_ENOSPC = 28,
    LINUX_EROFS = 30,
    LINUX_EPIPE = 32,
    LINUX_ENOSYS = 38,
    LINUX_ENOTEMPTY = 39,
    LINUX_ETIMEDOUT = 110,
    LINUX_ECANCELED = 125,
};

extern const unsigned char *const swipterm_guest_kernel_data;
extern const size_t swipterm_guest_kernel_size;
extern const unsigned char *const swipterm_guest_initramfs_data;
extern const size_t swipterm_guest_initramfs_size;

int swipterm_upper_create(const char *path);

typedef struct swipterm_upper swipterm_upper_t;

typedef struct ext4_xattr_info {
    const char *name;
    const void *value;
    size_t size;
} ext4_xattr_info_t;

typedef struct ext4_entry_info {
    const char *name;
    size_t name_len;
    uint32_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t nlink;
    uint64_t size;
    uint64_t rdev;
    int64_t mtime;
    uint32_t mtime_nsec;
    bool opaque_dir;
    const char *symlink_target;
    const ext4_xattr_info_t *xattrs;
    size_t xattr_count;
} ext4_entry_info_t;

typedef int (*ext4_walk_cb)(void *ctx, void *parent, const ext4_entry_info_t *entry, void **child_out);
typedef int (*ext4_data_cb)(void *ctx, const void *data, size_t length);

int swipterm_upper_open(const char *path, swipterm_upper_t **upper_out);
int swipterm_upper_walk(swipterm_upper_t *upper, void *root, ext4_walk_cb cb, void *ctx);
int swipterm_upper_read(swipterm_upper_t *upper, uint32_t ino, ext4_data_cb cb, void *ctx);
void swipterm_upper_close(swipterm_upper_t *upper);

void slirp_reset_dns_cache(void);

void swipterm_dax_register(const uint8_t uuid[16], const void *data, size_t size);
void swipterm_dax_unregister(const uint8_t uuid[16]);
bool swipterm_dax_lookup(const uint8_t uuid[16], const void **data_out, size_t *size_out);
bool swipterm_root_read_dax_uuid(const char *root_path, uint8_t uuid_out[16]);

typedef struct swipterm_mirror swipterm_mirror_t;

swipterm_mirror_t *swipterm_mirror_create(virtio_dev_t *vsock, uint64_t session_nonce);
void swipterm_mirror_set_nonce(swipterm_mirror_t *mirror, uint64_t session_nonce);
void swipterm_mirror_apply_updates(swipterm_mirror_t *mirror,
                                   const swiptermd_mirror_entry_t *entries,
                                   size_t count);
void swipterm_mirror_destroy(swipterm_mirror_t *mirror);

#endif
