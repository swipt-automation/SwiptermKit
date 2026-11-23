#ifndef VMM_HOST_INTERNAL_H
#define VMM_HOST_INTERNAL_H

#include "../../Guest/swiptermd/protocol.h"
#include "../include/CSwiptermKit/swipterm.h"
#include "../virtio/virtio_mmio.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

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

void swipterm_network_set_runtime(swipterm_network *network, virtio_dev_t *vsock);
void swipterm_network_set_session(swipterm_network *network, uint64_t session_nonce);
void swipterm_network_push_listeners(swipterm_network *network, const swiptermd_mirror_entry_t *entries,
                                     size_t count);

// The iOS and macOS SDKs ship libarchive and liblzma without headers.

typedef int64_t la_int64_t;
typedef ssize_t la_ssize_t;

struct archive;
struct archive_entry;

enum {
    ARCHIVE_EOF = 1,
    ARCHIVE_OK = 0,
    ARCHIVE_RETRY = -10,
    ARCHIVE_WARN = -20,
    ARCHIVE_FAILED = -25,
    ARCHIVE_FATAL = -30,
    ARCHIVE_ERRNO_MISC = -1,
};

enum {
    ARCHIVE_ENTRY_ACL_EXECUTE = 0x00000001,
    ARCHIVE_ENTRY_ACL_WRITE = 0x00000002,
    ARCHIVE_ENTRY_ACL_READ = 0x00000004,
    ARCHIVE_ENTRY_ACL_TYPE_ACCESS = 0x00000100,
    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT = 0x00000200,
    ARCHIVE_ENTRY_ACL_USER = 10001,
    ARCHIVE_ENTRY_ACL_USER_OBJ = 10002,
    ARCHIVE_ENTRY_ACL_GROUP = 10003,
    ARCHIVE_ENTRY_ACL_GROUP_OBJ = 10004,
    ARCHIVE_ENTRY_ACL_MASK = 10005,
    ARCHIVE_ENTRY_ACL_OTHER = 10006,
};

typedef la_ssize_t archive_read_callback(struct archive *, void *_client_data, const void **_buffer);
typedef int archive_open_callback(struct archive *, void *_client_data);
typedef int archive_close_callback(struct archive *, void *_client_data);

extern struct archive *archive_read_new(void);
extern int archive_read_support_filter_all(struct archive *);
extern int archive_read_support_filter_none(struct archive *);
extern int archive_read_support_format_tar(struct archive *);
extern int archive_read_open(struct archive *, void *_client_data, archive_open_callback *,
                             archive_read_callback *, archive_close_callback *);
extern int archive_read_open_filename(struct archive *, const char *filename, size_t block_size);
extern int archive_read_open_memory(struct archive *, const void *buff, size_t size);
extern int archive_read_next_header(struct archive *, struct archive_entry **);
extern int archive_read_data_block(struct archive *, const void **buff, size_t *size, la_int64_t *offset);
extern int archive_read_free(struct archive *);
extern la_int64_t archive_filter_bytes(struct archive *, int);
extern const char *archive_error_string(struct archive *);
extern void archive_set_error(struct archive *, int _err, const char *fmt, ...);

extern struct archive *archive_write_new(void);
extern int archive_write_add_filter_gzip(struct archive *);
extern int archive_write_set_format_pax_restricted(struct archive *);
extern int archive_write_open_fd(struct archive *, int fd);
extern int archive_write_header(struct archive *, struct archive_entry *);
extern la_ssize_t archive_write_data(struct archive *, const void *, size_t);
extern int archive_write_close(struct archive *);
extern int archive_write_fail(struct archive *);
extern int archive_write_free(struct archive *);

extern struct archive_entry *archive_entry_new(void);
extern void archive_entry_free(struct archive_entry *);
extern const char *archive_entry_pathname(struct archive_entry *);
extern mode_t archive_entry_mode(struct archive_entry *);
extern la_int64_t archive_entry_uid(struct archive_entry *);
extern la_int64_t archive_entry_gid(struct archive_entry *);
extern time_t archive_entry_mtime(struct archive_entry *);
extern long archive_entry_mtime_nsec(struct archive_entry *);
extern la_int64_t archive_entry_size(struct archive_entry *);
extern int archive_entry_size_is_set(struct archive_entry *);
extern unsigned int archive_entry_rdevmajor(struct archive_entry *);
extern unsigned int archive_entry_rdevminor(struct archive_entry *);
extern const char *archive_entry_symlink(struct archive_entry *);
extern const char *archive_entry_hardlink(struct archive_entry *);
extern int archive_entry_xattr_count(struct archive_entry *);
extern int archive_entry_xattr_reset(struct archive_entry *);
extern int archive_entry_xattr_next(struct archive_entry *, const char **name, const void **value, size_t *size);
extern void archive_entry_xattr_add_entry(struct archive_entry *, const char *name, const void *value, size_t size);
extern int archive_entry_acl_reset(struct archive_entry *, int want_type);
extern int archive_entry_acl_next(struct archive_entry *, int want_type, int *type, int *permset, int *tag,
                                  int *qual, const char **name);
extern int archive_entry_acl_add_entry(struct archive_entry *, int type, int permset, int tag, int qual,
                                       const char *name);

extern void archive_entry_set_pathname(struct archive_entry *, const char *);
extern void archive_entry_set_mode(struct archive_entry *, mode_t);
extern void archive_entry_set_uid(struct archive_entry *, la_int64_t);
extern void archive_entry_set_gid(struct archive_entry *, la_int64_t);
extern void archive_entry_set_size(struct archive_entry *, la_int64_t);
extern void archive_entry_set_mtime(struct archive_entry *, time_t, long);
extern void archive_entry_set_rdevmajor(struct archive_entry *, unsigned int);
extern void archive_entry_set_rdevminor(struct archive_entry *, unsigned int);
extern void archive_entry_set_symlink(struct archive_entry *, const char *);
extern void archive_entry_set_hardlink(struct archive_entry *, const char *);
extern void archive_entry_sparse_add_entry(struct archive_entry *, la_int64_t offset, la_int64_t length);

typedef enum {
    LZMA_RESERVED_ENUM = 0,
} lzma_reserved_enum;

typedef enum {
    LZMA_OK = 0,
    LZMA_STREAM_END = 1,
    LZMA_BUF_ERROR = 10,
} lzma_ret;

typedef enum {
    LZMA_RUN = 0,
    LZMA_FINISH = 3,
} lzma_action;

typedef enum {
    LZMA_CHECK_NONE = 0,
} lzma_check;

typedef struct lzma_allocator lzma_allocator;
typedef struct lzma_internal_s lzma_internal;
typedef struct lzma_filter lzma_filter;

typedef struct {
    const uint8_t *next_in;
    size_t avail_in;
    uint64_t total_in;
    uint8_t *next_out;
    size_t avail_out;
    uint64_t total_out;
    const lzma_allocator *allocator;
    lzma_internal *internal;
    void *reserved_ptr1;
    void *reserved_ptr2;
    void *reserved_ptr3;
    void *reserved_ptr4;
    uint64_t seek_pos;
    uint64_t reserved_int2;
    size_t reserved_int3;
    size_t reserved_int4;
    lzma_reserved_enum reserved_enum1;
    lzma_reserved_enum reserved_enum2;
} lzma_stream;

typedef struct {
    uint32_t flags;
    uint32_t threads;
    uint64_t block_size;
    uint32_t timeout;
    uint32_t preset;
    const lzma_filter *filters;
    lzma_check check;
    lzma_reserved_enum reserved_enum1;
    lzma_reserved_enum reserved_enum2;
    lzma_reserved_enum reserved_enum3;
    uint32_t reserved_int1;
    uint32_t reserved_int2;
    uint32_t reserved_int3;
    uint32_t reserved_int4;
    uint64_t memlimit_threading;
    uint64_t memlimit_stop;
    uint64_t reserved_int7;
    uint64_t reserved_int8;
    void *reserved_ptr1;
    void *reserved_ptr2;
    void *reserved_ptr3;
    void *reserved_ptr4;
} lzma_mt;

typedef unsigned char lzma_bool;
typedef uint64_t lzma_vli;
typedef struct lzma_index_s lzma_index;

typedef struct {
    uint32_t version;
    lzma_vli backward_size;
    lzma_check check;
    lzma_reserved_enum reserved_enum1;
    lzma_reserved_enum reserved_enum2;
    lzma_reserved_enum reserved_enum3;
    lzma_reserved_enum reserved_enum4;
    lzma_bool reserved_bool1;
    lzma_bool reserved_bool2;
    lzma_bool reserved_bool3;
    lzma_bool reserved_bool4;
    lzma_bool reserved_bool5;
    lzma_bool reserved_bool6;
    lzma_bool reserved_bool7;
    lzma_bool reserved_bool8;
    uint32_t reserved_int1;
    uint32_t reserved_int2;
} lzma_stream_flags;

#define LZMA_CONCATENATED UINT32_C(0x08)
#define LZMA_STREAM_HEADER_SIZE 12

extern lzma_ret lzma_stream_decoder_mt(lzma_stream *strm, const lzma_mt *options);
extern lzma_ret lzma_auto_decoder(lzma_stream *strm, uint64_t memlimit, uint32_t flags);
extern lzma_ret lzma_code(lzma_stream *strm, lzma_action action);
extern void lzma_end(lzma_stream *strm);
extern uint32_t lzma_cputhreads(void);
extern lzma_ret lzma_stream_footer_decode(lzma_stream_flags *options, const uint8_t *in);
extern lzma_ret lzma_index_buffer_decode(lzma_index **i, uint64_t *memlimit, const lzma_allocator *allocator,
                                         const uint8_t *in, size_t *in_pos, size_t in_size);
extern lzma_vli lzma_index_stream_size(const lzma_index *i);
extern lzma_vli lzma_index_uncompressed_size(const lzma_index *i);
extern void lzma_index_end(lzma_index *i, const lzma_allocator *allocator);

static_assert(sizeof(lzma_stream_flags) == 56);
static_assert(offsetof(lzma_stream_flags, backward_size) == 8);
static_assert(offsetof(lzma_stream_flags, check) == 16);

int swipterm_decompress(const uint8_t *in, size_t len, size_t limit, uint8_t **out, size_t *out_len);
int swipterm_kernel_image(const uint8_t *file, size_t len, uint8_t **image_out, size_t *image_len);
int swipterm_kernel_probe(const uint8_t *file_prefix, size_t len, size_t *payload_offset_out);
int swipterm_kernel_check_header(const uint8_t *image, size_t len);
bool swipterm_kernel_find_release(const uint8_t *image, size_t len, char *release, size_t release_size);

typedef struct ext4_fs ext4_fs_t;

typedef struct ext4_node {
    uint32_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t nlink;
    uint32_t generation;
    uint64_t size;
    int64_t mtime_ns;
    int64_t ctime_ns;
} ext4_node_t;

typedef struct ext4_xattr_info {
    const char *name;
    const void *value;
    size_t size;
} ext4_xattr_info_t;

typedef struct ext4_entry_info {
    ext4_node_t node;
    uint64_t rdev_major;
    uint64_t rdev_minor;
    int64_t mtime;
    uint32_t mtime_nsec;
    const char *symlink_target;
    const ext4_xattr_info_t *xattrs;
    size_t xattr_count;
} ext4_entry_info_t;

typedef int (*ext4_data_cb)(void *ctx, const void *data, size_t length);
typedef int (*ext4_list_cb)(void *ctx, const char *name, size_t name_len, uint32_t ino);
typedef int (*ext4_extent_cb)(void *ctx, uint64_t offset, uint64_t length);
typedef int (*ext4_walk_cb)(void *ctx, const char *path, const ext4_entry_info_t *entry);

int ext4_fs_open(int fd, ext4_fs_t **fs_out);
void ext4_fs_close(ext4_fs_t *fs);
uint64_t ext4_fs_used_inodes(const ext4_fs_t *fs);
int ext4_fs_stat(ext4_fs_t *fs, uint32_t ino, ext4_node_t *node_out);
int ext4_fs_resolve(ext4_fs_t *fs, const char *path, bool follow_final, ext4_node_t *node_out);
int ext4_fs_list(ext4_fs_t *fs, const ext4_node_t *dir, ext4_list_cb cb, void *ctx);
int ext4_fs_stream(ext4_fs_t *fs, const ext4_node_t *node, uint64_t offset, uint64_t length,
                   ext4_data_cb cb, void *ctx);
int ext4_fs_read(ext4_fs_t *fs, const ext4_node_t *node, size_t limit, uint8_t **data_out, size_t *size_out);
int ext4_fs_map(ext4_fs_t *fs, const ext4_node_t *node, ext4_extent_cb cb, void *ctx);
int ext4_fs_walk(ext4_fs_t *fs, ext4_walk_cb cb, void *ctx);

typedef struct vfs_xattr {
    struct vfs_xattr *next;
    uint8_t index;
    uint8_t name_len;
    uint16_t value_len;
    uint8_t payload[];
} vfs_xattr_t;

typedef struct vfs_extent {
    uint64_t logical;
    uint64_t physical;
    uint32_t length;
} vfs_extent_t;

typedef struct vfs_node vfs_node_t;

typedef struct vfs_dirent {
    char *name;
    uint32_t hash;
    uint16_t name_len;
    vfs_node_t *node;
} vfs_dirent_t;

struct vfs_node {
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    int64_t mtime;
    uint32_t mtime_nsec;
    uint32_t rdev_major;
    uint32_t rdev_minor;
    uint32_t refcount;
    uint32_t nlink;
    uint32_t ino;
    bool implicit_dir;

    uint64_t size;
    char *symlink;
    vfs_xattr_t *xattrs;

    vfs_extent_t *extents;
    size_t extent_count;
    size_t extent_cap;

    vfs_dirent_t *children;
    uint32_t *hash_slots;
    size_t child_count;
    size_t child_cap;
    size_t hash_cap;
};

vfs_node_t *vfs_node_new(uint32_t mode);
vfs_node_t *vfs_node_retain(vfs_node_t *node);
void vfs_node_release(vfs_node_t *node);
bool vfs_dir_set(vfs_node_t *dir, const char *name, size_t name_len, vfs_node_t *child);
vfs_dirent_t *vfs_dir_find(vfs_node_t *dir, const char *name, size_t name_len);

typedef struct ext4_writer ext4_writer_t;

int ext4_writer_create(int fd, uint64_t disk_size, ext4_writer_t **writer_out);
int ext4_writer_write(ext4_writer_t *w, vfs_node_t *file, uint64_t offset, const void *data, size_t length);
int ext4_writer_end_file(ext4_writer_t *w, vfs_node_t *file, uint64_t size);
int ext4_writer_finish(ext4_writer_t *w, vfs_node_t *root);
void ext4_writer_free(ext4_writer_t *w);

typedef struct boot_plan boot_plan_t;

int boot_plan_resolve(int disk_fd, const char *bundled_kernel_path, swipterm_kernel_selection selection,
                      const char *release, boot_plan_t **plan_out);
bool boot_plan_is_bundled(const boot_plan_t *plan);
const char *boot_plan_release(const boot_plan_t *plan);
const char *boot_plan_fallback_reason(const boot_plan_t *plan);
const uint8_t *boot_plan_identity(const boot_plan_t *plan);
int boot_plan_load(boot_plan_t *plan, uint8_t **kernel_out, size_t *kernel_size_out,
                   uint8_t **modules_cpio_out, size_t *modules_cpio_size_out);
int boot_plan_next(boot_plan_t *plan, const char *reason);
void boot_plan_free(boot_plan_t *plan);

#endif
