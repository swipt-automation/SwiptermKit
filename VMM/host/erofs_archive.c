#define _DARWIN_C_SOURCE

#include "host_internal.h"
#include "../include/CSwiptermKit/swipterm.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if __has_include(<archive.h>) && __has_include(<archive_entry.h>)
#include <archive.h>
#include <archive_entry.h>
#else
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

typedef la_ssize_t archive_read_callback(struct archive *, void *_client_data, const void **_buffer);
typedef int archive_open_callback(struct archive *, void *_client_data);
typedef int archive_close_callback(struct archive *, void *_client_data);

extern int archive_read_open(struct archive *, void *_client_data, archive_open_callback *,
                             archive_read_callback *, archive_close_callback *);
extern void archive_set_error(struct archive *, int _err, const char *fmt, ...);
extern struct archive *archive_read_new(void);
extern int archive_read_support_filter_none(struct archive *);
extern int archive_read_support_filter_gzip(struct archive *);
extern int archive_read_support_filter_bzip2(struct archive *);
extern int archive_read_support_filter_xz(struct archive *);
extern int archive_read_support_filter_lzma(struct archive *);
extern int archive_read_support_filter_compress(struct archive *);
extern int archive_read_support_filter_zstd(struct archive *);
extern const char *archive_libzstd_version(void);
extern int archive_read_support_format_tar(struct archive *);
extern int archive_read_open_filename(struct archive *, const char *filename, size_t block_size);
extern int archive_read_open_memory(struct archive *, const void *buff, size_t size);
extern int archive_read_next_header(struct archive *, struct archive_entry **);
extern la_ssize_t archive_read_data(struct archive *, void *, size_t);
extern int archive_read_free(struct archive *);
extern la_int64_t archive_filter_bytes(struct archive *, int);
extern const char *archive_error_string(struct archive *);

extern struct archive *archive_write_new(void);
extern int archive_write_add_filter_gzip(struct archive *);
extern int archive_write_set_format_pax_restricted(struct archive *);
extern int archive_write_open_filename(struct archive *, const char *filename);
extern int archive_write_header(struct archive *, struct archive_entry *);
extern la_ssize_t archive_write_data(struct archive *, const void *, size_t);
extern int archive_write_close(struct archive *);
extern int archive_write_free(struct archive *);

extern struct archive_entry *archive_entry_new(void);
extern void archive_entry_free(struct archive_entry *);
extern const char *archive_entry_pathname(struct archive_entry *);
extern mode_t archive_entry_mode(struct archive_entry *);
extern la_int64_t archive_entry_uid(struct archive_entry *);
extern la_int64_t archive_entry_gid(struct archive_entry *);
extern la_int64_t archive_entry_size(struct archive_entry *);
extern time_t archive_entry_mtime(struct archive_entry *);
extern long archive_entry_mtime_nsec(struct archive_entry *);
extern unsigned int archive_entry_rdevmajor(struct archive_entry *);
extern unsigned int archive_entry_rdevminor(struct archive_entry *);
extern const char *archive_entry_symlink(struct archive_entry *);
extern const char *archive_entry_hardlink(struct archive_entry *);
extern int archive_entry_xattr_count(struct archive_entry *);
extern int archive_entry_xattr_reset(struct archive_entry *);
extern int archive_entry_xattr_next(struct archive_entry *, const char **name, const void **value, size_t *size);
extern void archive_entry_xattr_add_entry(struct archive_entry *, const char *name, const void *value, size_t size);

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
#endif

#if __has_include(<lzma.h>)
#include <lzma.h>
#else
typedef enum {
    LZMA_RESERVED_ENUM = 0,
} lzma_reserved_enum;

typedef enum {
    LZMA_OK = 0,
    LZMA_STREAM_END = 1,
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
extern lzma_ret lzma_code(lzma_stream *strm, lzma_action action);
extern void lzma_end(lzma_stream *strm);
extern uint32_t lzma_cputhreads(void);
extern lzma_ret lzma_stream_footer_decode(lzma_stream_flags *options, const uint8_t *in);
extern lzma_ret lzma_index_buffer_decode(lzma_index **i, uint64_t *memlimit, const lzma_allocator *allocator,
                                         const uint8_t *in, size_t *in_pos, size_t in_size);
extern lzma_vli lzma_index_stream_size(const lzma_index *i);
extern lzma_vli lzma_index_uncompressed_size(const lzma_index *i);
extern void lzma_index_end(lzma_index *i, const lzma_allocator *allocator);
#endif

static_assert(LZMA_STREAM_HEADER_SIZE == 12);
static_assert(sizeof(lzma_stream_flags) == 56);
static_assert(offsetof(lzma_stream_flags, backward_size) == 8);
static_assert(offsetof(lzma_stream_flags, check) == 16);

#ifdef SWIPTERM_HAS_ZSTD
#include <zstd.h>
#endif

enum : uint32_t {
    EROFS_SUPER_MAGIC_V1 = 0xE0F5E1E2U,
    EROFS_SUPER_OFFSET = 1024U,
    EROFS_BLOCK_SIZE = 4096U,
    EROFS_BLKSZBITS = 12U,
    EROFS_ALIGN_SIZE = 16384U,
    EROFS_ISLOTBITS = 5U,
    EROFS_ISLOT_SIZE = 32U,
    EROFS_INODE_COMPACT = 0U,
    EROFS_INODE_EXTENDED = 1U,
    EROFS_INODE_FLAT_PLAIN = 0U,
    EROFS_INODE_FLAT_INLINE = 2U,
    EROFS_DIRENT_SIZE = 12U,
    EROFS_XATTR_IBODY_HDR_SIZE = 12U,
    EROFS_XATTR_ENTRY_HDR_SIZE = 4U,
    EROFS_MAX_INLINE_XATTR_BYTES = 3072U,

    STREAM_IO_BUF_SIZE = 65536U,
    EROFS_WRITE_BUF_SIZE = 1U << 20,
    XZ_MT_BUF_SIZE = 1U << 20,
    XZ_TAIL_SIZE = 4096U,
    XZ_INDEX_MAX_SIZE = 4U << 20,
    VFS_MAX_DEPTH = 128U,
    VFS_MAX_SYMLINKS = 40U,
    VFS_MAX_NAME_LEN = 255U,
    VFS_MAX_SYMLINK_LEN = 4096U,
    VFS_HASH_THRESHOLD = 32U,
    /* Safety caps to prevent OOM/disk exhaustion on malicious archives */
    VFS_MAX_ENTRIES = 2000000U,
    VFS_MAX_IMAGE_BLOCKS = 0x3FFFFFFFU, /* ~4 TiB max blocks (< UINT32_MAX) */
};

enum : uint8_t {
    EROFS_FT_UNKNOWN = 0,
    EROFS_FT_REG_FILE = 1,
    EROFS_FT_DIR = 2,
    EROFS_FT_CHRDEV = 3,
    EROFS_FT_BLKDEV = 4,
    EROFS_FT_FIFO = 5,
    EROFS_FT_SOCK = 6,
    EROFS_FT_SYMLINK = 7,
};

enum : uint8_t {
    EROFS_XATTR_INDEX_USER = 1,
    EROFS_XATTR_INDEX_POSIX_ACL_ACCESS = 2,
    EROFS_XATTR_INDEX_POSIX_ACL_DEFAULT = 3,
    EROFS_XATTR_INDEX_TRUSTED = 4,
    EROFS_XATTR_INDEX_SECURITY = 6,
};

static inline void put_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static inline void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline void put_le64(uint8_t *p, uint64_t v) {
    put_le32(p, (uint32_t)v);
    put_le32(p + 4, (uint32_t)(v >> 32));
}

static inline uint16_t get_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t get_le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline uint64_t get_le64(const uint8_t *p) {
    return (uint64_t)get_le32(p) | ((uint64_t)get_le32(p + 4) << 32);
}

static void set_error(char **error_message, const char *msg) {
    if (error_message) {
        *error_message = strdup(msg ? msg : "unknown error");
    }
}

/* =========================================================================
 * DAX Registry (with unregister support for same-process reset/cleanup)
 * ========================================================================= */

typedef struct dax_entry {
    uint8_t uuid[16];
    const void *data;
    size_t size;
    struct dax_entry *next;
} dax_entry_t;

static pthread_mutex_t g_dax_mutex = PTHREAD_MUTEX_INITIALIZER;
static dax_entry_t *g_dax_entries = NULL;

void swipterm_dax_register(const uint8_t uuid[16], const void *data, size_t size) {
    if (!data || size == 0) {
        return;
    }
    pthread_mutex_lock(&g_dax_mutex);
    for (dax_entry_t *e = g_dax_entries; e != NULL; e = e->next) {
        if (memcmp(e->uuid, uuid, 16) == 0) {
            e->data = data;
            e->size = size;
            pthread_mutex_unlock(&g_dax_mutex);
            return;
        }
    }
    dax_entry_t *e = calloc(1, sizeof(*e));
    if (e) {
        memcpy(e->uuid, uuid, 16);
        e->data = data;
        e->size = size;
        e->next = g_dax_entries;
        g_dax_entries = e;
    }
    pthread_mutex_unlock(&g_dax_mutex);
}

void swipterm_dax_unregister(const uint8_t uuid[16]) {
    pthread_mutex_lock(&g_dax_mutex);
    for (dax_entry_t **pp = &g_dax_entries; *pp != NULL; pp = &(*pp)->next) {
        if (memcmp((*pp)->uuid, uuid, 16) == 0) {
            dax_entry_t *dead = *pp;
            *pp = dead->next;
            free(dead);
            break;
        }
    }
    pthread_mutex_unlock(&g_dax_mutex);
}

bool swipterm_dax_lookup(const uint8_t uuid[16], const void **data_out, size_t *size_out) {
    pthread_mutex_lock(&g_dax_mutex);
    for (dax_entry_t *e = g_dax_entries; e != NULL; e = e->next) {
        if (memcmp(e->uuid, uuid, 16) == 0) {
            *data_out = e->data;
            *size_out = e->size;
            pthread_mutex_unlock(&g_dax_mutex);
            return true;
        }
    }
    pthread_mutex_unlock(&g_dax_mutex);
    return false;
}

static void format_uuid_hex(const uint8_t uuid[16], char out[33]) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        out[i * 2] = hex[uuid[i] >> 4];
        out[i * 2 + 1] = hex[uuid[i] & 0x0F];
    }
    out[32] = '\0';
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool parse_uuid_hex(const char *s, uint8_t uuid[16]) {
    for (int i = 0; i < 16; i++) {
        int hi = hex_nibble(s[i * 2]);
        int lo = hex_nibble(s[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        uuid[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

bool swipterm_root_read_dax_uuid(const char *root_path, uint8_t uuid_out[16]) {
    char meta_path[1024];
    int n = snprintf(meta_path, sizeof(meta_path), "%s/swipterm.json", root_path);
    if (n <= 0 || (size_t)n >= sizeof(meta_path)) {
        return false;
    }
    int fd = open(meta_path, O_RDONLY);
    if (fd < 0) {
        return false;
    }
    char buf[1024];
    ssize_t rd = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (rd <= 0) {
        return false;
    }
    buf[rd] = '\0';

    /* Only resolve DAX UUID when lower_mode is explicitly "dax" */
    const char *mode_key = strstr(buf, "\"lower_mode\"");
    if (mode_key) {
        const char *mcolon = strchr(mode_key, ':');
        const char *mq1 = mcolon ? strchr(mcolon, '"') : NULL;
        if (!mq1 || strncmp(mq1 + 1, "dax\"", 4) != 0) {
            return false;
        }
    }

    const char *key = strstr(buf, "\"lower_uuid\"");
    if (!key) {
        return false;
    }
    const char *colon = strchr(key, ':');
    if (!colon) {
        return false;
    }
    const char *q1 = strchr(colon, '"');
    if (!q1 || strlen(q1 + 1) < 32) {
        return false;
    }
    return parse_uuid_hex(q1 + 1, uuid_out);
}

static bool write_metadata_json(const char *root_path, const char *lower_mode,
                                const uint8_t uuid[16], size_t lower_size) {
    char meta_path[1024];
    int n = snprintf(meta_path, sizeof(meta_path), "%s/swipterm.json", root_path);
    if (n <= 0 || (size_t)n >= sizeof(meta_path)) {
        return false;
    }
    char hex_uuid[33];
    format_uuid_hex(uuid, hex_uuid);
    char json[512];
    int len = snprintf(json, sizeof(json),
                       "{\n"
                       "  \"version\": 1,\n"
                       "  \"lower_mode\": \"%s\",\n"
                       "  \"lower_uuid\": \"%s\",\n"
                       "  \"lower_size\": %zu,\n"
                       "  \"upper\": \"upper.ext4\"\n"
                       "}\n",
                       lower_mode, hex_uuid, lower_size);
    if (len <= 0 || (size_t)len >= sizeof(json)) {
        return false;
    }
    int fd = open(meta_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return false;
    }
    ssize_t wr = write(fd, json, (size_t)len);
    int rc = close(fd);
    return wr == len && rc == 0;
}

/* =========================================================================
 * Compact Streaming VFS Tree (No File Data or Dir Blocks in RAM!)
 * ========================================================================= */

typedef struct vfs_xattr {
    uint8_t name_index;
    uint8_t name_len;
    uint16_t value_len;
    struct vfs_xattr *next;
    uint8_t payload[]; /* name_len bytes + value_len bytes */
} vfs_xattr_t;

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
    uint64_t mtime;
    uint32_t mtime_nsec;
    uint32_t rdev_major;
    uint32_t rdev_minor;
    uint32_t refcount;
    uint32_t nlink;

    uint64_t size;
    char *symlink;
    vfs_xattr_t *xattrs;

    /* For import: regular file bytes are streamed straight to lower.erofs at erofs_raw_blkaddr.
     * For export: lower EROFS files reference mmap directly (export_from_lower) and upper
     * files are streamed from upper.ext4 by inode (export_from_upper). */
    uint64_t export_inline_off;
    uint32_t upper_ino;
    uint16_t export_datalayout;
    bool export_from_lower;
    bool export_from_upper;
    bool implicit_dir;

    vfs_dirent_t *children;
    uint32_t *hash_slots; /* 1-based index into children[], size = hash_cap */
    size_t child_count;
    size_t child_cap;
    size_t hash_cap;

    uint32_t erofs_ino;
    uint64_t erofs_nid;
    uint32_t erofs_raw_blkaddr;
    uint64_t erofs_dir_size;
    uint16_t erofs_xattr_icount;
    uint16_t erofs_xattr_bytes;

    char *exported_path;
};

static uint32_t fnv1a_32(const char *s, size_t len) {
    uint32_t h = 2166136261U;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)s[i];
        h *= 16777619U;
    }
    return h;
}

static vfs_node_t *vfs_node_new(uint32_t mode) {
    vfs_node_t *n = calloc(1, sizeof(*n));
    if (!n) {
        return NULL;
    }
    n->mode = mode;
    n->refcount = 1;
    n->mtime = (uint64_t)time(NULL);
    return n;
}

static vfs_node_t *vfs_node_retain(vfs_node_t *n) {
    if (n) {
        n->refcount++;
    }
    return n;
}

static void vfs_node_free_xattrs(vfs_node_t *n) {
    vfs_xattr_t *x = n->xattrs;
    while (x) {
        vfs_xattr_t *next = x->next;
        free(x);
        x = next;
    }
    n->xattrs = NULL;
}

/* Iterative release avoids call-stack overflow on deep directory trees (iOS 512 KiB stacks) */
static void vfs_node_release(vfs_node_t *root) {
    if (!root || --root->refcount > 0) {
        return;
    }
    size_t cap = 64;
    size_t top = 0;
    vfs_node_t **stack = malloc(cap * sizeof(*stack));
    if (!stack) {
        /* Fallback single-node free if OOM */
        vfs_node_free_xattrs(root);
        free(root->children);
        free(root->hash_slots);
        free(root->symlink);
        free(root->exported_path);
        free(root);
        return;
    }
    stack[top++] = root;
    while (top > 0) {
        vfs_node_t *n = stack[--top];
        for (size_t i = 0; i < n->child_count; i++) {
            free(n->children[i].name);
            vfs_node_t *child = n->children[i].node;
            if (child && --child->refcount == 0) {
                if (top == cap) {
                    size_t next_cap = cap * 2;
                    vfs_node_t **next = realloc(stack, next_cap * sizeof(*next));
                    if (next) {
                        stack = next;
                        cap = next_cap;
                    } else {
                        /* Emergency recursion if realloc fails during teardown */
                        child->refcount = 1;
                        vfs_node_release(child);
                        continue;
                    }
                }
                stack[top++] = child;
            }
        }
        vfs_node_free_xattrs(n);
        free(n->children);
        free(n->hash_slots);
        free(n->symlink);
        free(n->exported_path);
        free(n);
    }
    free(stack);
}

static void vfs_dir_clear(vfs_node_t *dir) {
    for (size_t i = 0; i < dir->child_count; i++) {
        free(dir->children[i].name);
        vfs_node_release(dir->children[i].node);
    }
    dir->child_count = 0;
    if (dir->hash_slots) {
        memset(dir->hash_slots, 0, dir->hash_cap * sizeof(uint32_t));
    }
}

static bool vfs_dir_rehash(vfs_node_t *dir, size_t min_cap) {
    size_t new_cap = 64;
    while (new_cap < min_cap * 2) {
        new_cap *= 2;
    }
    uint32_t *slots = calloc(new_cap, sizeof(uint32_t));
    if (!slots) {
        return false;
    }
    size_t mask = new_cap - 1;
    for (size_t i = 0; i < dir->child_count; i++) {
        size_t pos = dir->children[i].hash & mask;
        while (slots[pos] != 0) {
            pos = (pos + 1) & mask;
        }
        slots[pos] = (uint32_t)(i + 1);
    }
    free(dir->hash_slots);
    dir->hash_slots = slots;
    dir->hash_cap = new_cap;
    return true;
}

static ssize_t vfs_dir_find_index(const vfs_node_t *dir, const char *name, size_t nlen, uint32_t hash) {
    if (dir->hash_slots && dir->hash_cap > 0) {
        size_t mask = dir->hash_cap - 1;
        size_t pos = hash & mask;
        while (dir->hash_slots[pos] != 0) {
            size_t idx = (size_t)(dir->hash_slots[pos] - 1);
            const vfs_dirent_t *de = &dir->children[idx];
            if (de->hash == hash && de->name_len == nlen && memcmp(de->name, name, nlen) == 0) {
                return (ssize_t)idx;
            }
            pos = (pos + 1) & mask;
        }
        return -1;
    }
    for (size_t i = 0; i < dir->child_count; i++) {
        const vfs_dirent_t *de = &dir->children[i];
        if (de->hash == hash && de->name_len == nlen && memcmp(de->name, name, nlen) == 0) {
            return (ssize_t)i;
        }
    }
    return -1;
}

static vfs_dirent_t *vfs_dir_find(vfs_node_t *dir, const char *name, size_t nlen) {
    uint32_t h = fnv1a_32(name, nlen);
    ssize_t idx = vfs_dir_find_index(dir, name, nlen, h);
    return idx >= 0 ? &dir->children[idx] : NULL;
}

static bool vfs_dir_remove(vfs_node_t *dir, const char *name, size_t nlen) {
    uint32_t h = fnv1a_32(name, nlen);
    ssize_t idx = vfs_dir_find_index(dir, name, nlen, h);
    if (idx < 0) {
        return false;
    }
    size_t i = (size_t)idx;
    free(dir->children[i].name);
    vfs_node_release(dir->children[i].node);
    if (i + 1 < dir->child_count) {
        dir->children[i] = dir->children[dir->child_count - 1];
    }
    dir->child_count--;
    if (dir->hash_slots) {
        if (dir->child_count < VFS_HASH_THRESHOLD / 2) {
            free(dir->hash_slots);
            dir->hash_slots = NULL;
            dir->hash_cap = 0;
        } else {
            (void)vfs_dir_rehash(dir, dir->child_count + 1);
        }
    }
    return true;
}

static bool vfs_dir_set(vfs_node_t *dir, const char *name, size_t nlen, vfs_node_t *child) {
    if (nlen == 0 || nlen > VFS_MAX_NAME_LEN) {
        return false;
    }
    uint32_t h = fnv1a_32(name, nlen);
    ssize_t idx = vfs_dir_find_index(dir, name, nlen, h);
    if (idx >= 0) {
        vfs_dirent_t *existing = &dir->children[idx];
        if (existing->node == child) {
            return true;
        }
        vfs_node_release(existing->node);
        existing->node = vfs_node_retain(child);
        return true;
    }
    if (dir->child_count == dir->child_cap) {
        size_t next_cap = dir->child_cap ? dir->child_cap * 2 : 8;
        vfs_dirent_t *next = realloc(dir->children, next_cap * sizeof(*next));
        if (!next) {
            return false;
        }
        dir->children = next;
        dir->child_cap = next_cap;
    }
    char *copy = malloc(nlen + 1);
    if (!copy) {
        return false;
    }
    memcpy(copy, name, nlen);
    copy[nlen] = '\0';
    size_t new_idx = dir->child_count++;
    dir->children[new_idx] = (vfs_dirent_t){
        .name = copy,
        .hash = h,
        .name_len = (uint16_t)nlen,
        .node = vfs_node_retain(child),
    };
    if (dir->hash_slots) {
        if (dir->child_count * 2 >= dir->hash_cap) {
            if (!vfs_dir_rehash(dir, dir->child_count + 1)) {
                free(dir->hash_slots);
                dir->hash_slots = NULL;
                dir->hash_cap = 0;
            }
        } else {
            size_t mask = dir->hash_cap - 1;
            size_t pos = h & mask;
            while (dir->hash_slots[pos] != 0) {
                pos = (pos + 1) & mask;
            }
            dir->hash_slots[pos] = (uint32_t)(new_idx + 1);
        }
    } else if (dir->child_count >= VFS_HASH_THRESHOLD) {
        (void)vfs_dir_rehash(dir, dir->child_count + 1);
    }
    return true;
}

/* =========================================================================
 * Symlink-Aware Path Resolution (Handles Merged-/usr & Relative Symlinks!)
 * ========================================================================= */

typedef struct vfs_path_stack {
    vfs_node_t *nodes[VFS_MAX_DEPTH + 1];
    size_t depth; /* nodes[0] == root, nodes[depth] == current dir */
} vfs_path_stack_t;

static bool vfs_walk_dir_path(vfs_node_t *root, vfs_path_stack_t *st, const char *path,
                              size_t path_len, bool create_dirs, unsigned *symlink_depth);

static bool vfs_follow_symlink_dir(vfs_node_t *root, vfs_path_stack_t *st,
                                   const char *target, bool create_dirs, unsigned *symlink_depth) {
    if (!target || target[0] == '\0') {
        return false;
    }
    if (*symlink_depth >= VFS_MAX_SYMLINKS) {
        return false;
    }
    (*symlink_depth)++;

    vfs_path_stack_t next_st = *st;
    const char *p = target;
    size_t len = strlen(p);
    if (p[0] == '/') {
        next_st.nodes[0] = root;
        next_st.depth = 0;
    }
    if (!vfs_walk_dir_path(root, &next_st, p, len, create_dirs, symlink_depth)) {
        return false;
    }
    *st = next_st;
    return true;
}

/* Walk EVERY component in path[0..path_len) expecting/producing a directory at st->nodes[st->depth] */
static bool vfs_walk_dir_path(vfs_node_t *root, vfs_path_stack_t *st, const char *path,
                              size_t path_len, bool create_dirs, unsigned *symlink_depth) {
    const char *p = path;
    const char *end = path + path_len;
    while (p < end) {
        while (p < end && *p == '/') {
            p++;
        }
        if (p >= end) {
            break;
        }
        const char *slash = memchr(p, '/', (size_t)(end - p));
        size_t clen = slash ? (size_t)(slash - p) : (size_t)(end - p);
        if (clen == 1 && p[0] == '.') {
            p = slash ? slash + 1 : end;
            continue;
        }
        if (clen == 2 && p[0] == '.' && p[1] == '.') {
            if (st->depth > 0) {
                st->depth--;
            }
            p = slash ? slash + 1 : end;
            continue;
        }
        if (clen > VFS_MAX_NAME_LEN) {
            return false;
        }
        vfs_node_t *cur = st->nodes[st->depth];
        if (!S_ISDIR(cur->mode)) {
            return false;
        }
        vfs_dirent_t *de = vfs_dir_find(cur, p, clen);
        if (!de) {
            if (!create_dirs) {
                return false;
            }
            vfs_node_t *dir = vfs_node_new(S_IFDIR | 0755);
            if (!dir) {
                return false;
            }
            dir->implicit_dir = true;
            if (!vfs_dir_set(cur, p, clen, dir)) {
                vfs_node_release(dir);
                return false;
            }
            vfs_node_release(dir);
            de = vfs_dir_find(cur, p, clen);
        }
        if (S_ISLNK(de->node->mode)) {
            if (!vfs_follow_symlink_dir(root, st, de->node->symlink, create_dirs, symlink_depth)) {
                return false;
            }
        } else if (S_ISDIR(de->node->mode)) {
            if (st->depth >= VFS_MAX_DEPTH) {
                return false;
            }
            st->nodes[++st->depth] = de->node;
        } else {
            return false;
        }
        p = slash ? slash + 1 : end;
    }
    return true;
}

/* Resolve parent directory (following intermediate symlinks like /bin -> usr/bin!) and return leaf.
 * Rejects '..' attempts that would escape above root.
 * Also outputs the parent stack so late-symlink merging can resolve relative targets! */
static bool vfs_resolve_parent_ex(vfs_node_t *root, const char *raw_path, bool create_parents,
                                  vfs_path_stack_t *parent_stack_out,
                                  vfs_node_t **parent_out, const char **leaf_out, size_t *leaf_len_out) {
    const char *p = raw_path;
    while (*p == '/' || (p[0] == '.' && p[1] == '/')) {
        p += (*p == '/') ? 1 : 2;
    }
    size_t len = strlen(p);
    while (len > 0 && p[len - 1] == '/') {
        len--;
    }
    if (len == 0 || (len == 1 && p[0] == '.')) {
        *parent_out = NULL;
        *leaf_out = NULL;
        *leaf_len_out = 0;
        if (parent_stack_out) {
            parent_stack_out->nodes[0] = root;
            parent_stack_out->depth = 0;
        }
        return true;
    }

    vfs_path_stack_t st;
    st.nodes[0] = root;
    st.depth = 0;
    unsigned sym_depth = 0;

    const char *end = p + len;
    while (p < end) {
        while (p < end && *p == '/') {
            p++;
        }
        if (p >= end) {
            break;
        }
        const char *slash = memchr(p, '/', (size_t)(end - p));
        size_t clen = slash ? (size_t)(slash - p) : (size_t)(end - p);
        if (clen == 1 && p[0] == '.') {
            p = slash ? slash + 1 : end;
            continue;
        }
        if (clen == 2 && p[0] == '.' && p[1] == '.') {
            if (st.depth == 0) {
                /* Reject escaping above root */
                return false;
            }
            st.depth--;
            p = slash ? slash + 1 : end;
            continue;
        }
        if (clen > VFS_MAX_NAME_LEN) {
            return false;
        }
        if (!slash) {
            vfs_node_t *cur = st.nodes[st.depth];
            if (!S_ISDIR(cur->mode)) {
                return false;
            }
            if (parent_stack_out) {
                *parent_stack_out = st;
            }
            *parent_out = cur;
            *leaf_out = p;
            *leaf_len_out = clen;
            return true;
        }
        vfs_node_t *cur = st.nodes[st.depth];
        if (!S_ISDIR(cur->mode)) {
            return false;
        }
        vfs_dirent_t *de = vfs_dir_find(cur, p, clen);
        if (!de) {
            if (!create_parents) {
                return false;
            }
            vfs_node_t *dir = vfs_node_new(S_IFDIR | 0755);
            if (!dir) {
                return false;
            }
            dir->implicit_dir = true;
            if (!vfs_dir_set(cur, p, clen, dir)) {
                vfs_node_release(dir);
                return false;
            }
            vfs_node_release(dir);
            de = vfs_dir_find(cur, p, clen);
        }
        if (S_ISLNK(de->node->mode)) {
            if (!vfs_follow_symlink_dir(root, &st, de->node->symlink, create_parents, &sym_depth)) {
                return false;
            }
        } else if (S_ISDIR(de->node->mode)) {
            if (st.depth >= VFS_MAX_DEPTH) {
                return false;
            }
            st.nodes[++st.depth] = de->node;
        } else {
            return false;
        }
        p = slash + 1;
    }

    *parent_out = NULL;
    *leaf_out = NULL;
    *leaf_len_out = 0;
    if (parent_stack_out) {
        *parent_stack_out = st;
    }
    return true;
}

static bool vfs_resolve_parent(vfs_node_t *root, const char *raw_path, bool create_parents,
                               vfs_node_t **parent_out, const char **leaf_out, size_t *leaf_len_out) {
    return vfs_resolve_parent_ex(root, raw_path, create_parents, NULL, parent_out, leaf_out, leaf_len_out);
}

static vfs_node_t *vfs_lookup(vfs_node_t *root, const char *raw_path) {
    vfs_node_t *parent = NULL;
    const char *leaf = NULL;
    size_t leaf_len = 0;
    if (!vfs_resolve_parent(root, raw_path, false, &parent, &leaf, &leaf_len)) {
        return NULL;
    }
    if (!parent) {
        return root;
    }
    vfs_dirent_t *de = vfs_dir_find(parent, leaf, leaf_len);
    return de ? de->node : NULL;
}

/* Merge children of src_dir into dst_dir (used when a late merged-/usr symlink replaces an implicit dir).
 * Existing non-directory entries in dst_dir win over src_dir duplicates; directories merge recursively. */
static bool vfs_dir_merge(vfs_node_t *dst, vfs_node_t *src, unsigned depth) {
    if (dst == src || depth > VFS_MAX_DEPTH) {
        return true;
    }
    for (size_t i = 0; i < src->child_count; i++) {
        const vfs_dirent_t *se = &src->children[i];
        vfs_dirent_t *de = vfs_dir_find(dst, se->name, se->name_len);
        if (!de) {
            if (!vfs_dir_set(dst, se->name, se->name_len, se->node)) {
                return false;
            }
        } else if (S_ISDIR(de->node->mode) && S_ISDIR(se->node->mode)) {
            if (!vfs_dir_merge(de->node, se->node, depth + 1)) {
                return false;
            }
        }
    }
    return true;
}

/* =========================================================================
 * Extended Attributes (PAX SCHILY.xattr.* -> EROFS Inline Xattrs)
 * ========================================================================= */

static bool map_xattr_prefix(const char *full_name, uint8_t *index_out, const char **suffix_out) {
    if (strncmp(full_name, "security.", 9) == 0) {
        *index_out = EROFS_XATTR_INDEX_SECURITY;
        *suffix_out = full_name + 9;
        return true;
    }
    if (strncmp(full_name, "user.", 5) == 0) {
        *index_out = EROFS_XATTR_INDEX_USER;
        *suffix_out = full_name + 5;
        return true;
    }
    if (strncmp(full_name, "trusted.", 8) == 0) {
        *index_out = EROFS_XATTR_INDEX_TRUSTED;
        *suffix_out = full_name + 8;
        return true;
    }
    if (strcmp(full_name, "system.posix_acl_access") == 0) {
        *index_out = EROFS_XATTR_INDEX_POSIX_ACL_ACCESS;
        *suffix_out = "";
        return true;
    }
    if (strcmp(full_name, "system.posix_acl_default") == 0) {
        *index_out = EROFS_XATTR_INDEX_POSIX_ACL_DEFAULT;
        *suffix_out = "";
        return true;
    }
    return false;
}

static vfs_xattr_t *vfs_xattr_new(uint8_t idx, const char *suffix, size_t slen, const void *val, size_t vlen) {
    vfs_xattr_t *xa = malloc(sizeof(*xa) + slen + vlen);
    if (!xa) {
        return NULL;
    }
    xa->name_index = idx;
    xa->name_len = (uint8_t)slen;
    xa->value_len = (uint16_t)vlen;
    xa->next = NULL;
    if (slen > 0) {
        memcpy(xa->payload, suffix, slen);
    }
    if (vlen > 0 && val) {
        memcpy(xa->payload + slen, val, vlen);
    }
    return xa;
}

static bool vfs_node_add_xattr(vfs_node_t *node, const char *full_name, const void *val, size_t vlen) {
    uint8_t idx = 0;
    const char *suffix = NULL;
    if (!full_name || !map_xattr_prefix(full_name, &idx, &suffix)) {
        return true; /* Silently ignore non-Linux xattrs like com.apple.* */
    }
    size_t slen = strlen(suffix);
    if (slen > 255 || vlen > 2048) {
        return true;
    }
    size_t entry_aligned = (EROFS_XATTR_ENTRY_HDR_SIZE + slen + vlen + 3U) & ~3U;
    size_t cur_bytes = node->erofs_xattr_bytes ? node->erofs_xattr_bytes : EROFS_XATTR_IBODY_HDR_SIZE;
    if (cur_bytes + entry_aligned > EROFS_MAX_INLINE_XATTR_BYTES) {
        return true; /* Cap inline xattr size safely */
    }

    vfs_xattr_t *xa = vfs_xattr_new(idx, suffix, slen, val, vlen);
    if (!xa) {
        return false;
    }
    xa->next = node->xattrs;
    node->xattrs = xa;
    node->erofs_xattr_bytes = (uint16_t)(cur_bytes + entry_aligned);
    node->erofs_xattr_icount = (uint16_t)(((node->erofs_xattr_bytes - EROFS_XATTR_IBODY_HDR_SIZE) / 4U) + 1U);
    return true;
}

static bool capture_entry_xattrs(struct archive_entry *entry, vfs_node_t *node) {
    int count = archive_entry_xattr_count(entry);
    if (count <= 0) {
        return true;
    }
    vfs_node_free_xattrs(node);
    node->erofs_xattr_bytes = 0;
    node->erofs_xattr_icount = 0;
    archive_entry_xattr_reset(entry);
    const char *name = NULL;
    const void *val = NULL;
    size_t vlen = 0;
    while (archive_entry_xattr_next(entry, &name, &val, &vlen) == ARCHIVE_OK) {
        if (!vfs_node_add_xattr(node, name, val, vlen)) {
            return false;
        }
    }
    return true;
}

/* =========================================================================
 * Buffered Sequential & Positional I/O Helpers
 * ========================================================================= */

static bool write_all_fd(int fd, const void *buf, size_t len) {
    const uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return true;
}

static bool pwrite_all_fd(int fd, const void *buf, size_t len, off_t off) {
    const uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = pwrite(fd, p, len, off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        p += (size_t)n;
        len -= (size_t)n;
        off += (off_t)n;
    }
    return true;
}

typedef struct buf_writer {
    int fd;
    uint8_t *buf;
    size_t cap;
    size_t used;
    uint64_t total_written;
} buf_writer_t;

static bool bw_flush(buf_writer_t *bw) {
    if (bw->used == 0) {
        return true;
    }
    if (!write_all_fd(bw->fd, bw->buf, bw->used)) {
        return false;
    }
    bw->total_written += bw->used;
    bw->used = 0;
    return true;
}

static bool bw_write(buf_writer_t *bw, const void *data, size_t len) {
    const uint8_t *p = data;
    while (len > 0) {
        size_t space = bw->cap - bw->used;
        if (space == 0) {
            if (!bw_flush(bw)) {
                return false;
            }
            space = bw->cap;
        }
        size_t chunk = len < space ? len : space;
        memcpy(bw->buf + bw->used, p, chunk);
        bw->used += chunk;
        p += chunk;
        len -= chunk;
    }
    return true;
}

static bool bw_write_zeros(buf_writer_t *bw, size_t len) {
    static const uint8_t zeros[EROFS_BLOCK_SIZE] = {0};
    while (len > 0) {
        size_t chunk = len < sizeof(zeros) ? len : sizeof(zeros);
        if (!bw_write(bw, zeros, chunk)) {
            return false;
        }
        len -= chunk;
    }
    return true;
}

/* =========================================================================
 * Iterative Node Collection, Nlink Computation & Streaming EROFS Finalizer
 * ========================================================================= */

typedef struct node_list {
    vfs_node_t **items;
    vfs_node_t **parents; /* parent dir for directories (for "..") */
    size_t count;
    size_t cap;
    uint64_t next_nid;
} node_list_t;

static bool node_list_add(node_list_t *list, vfs_node_t *node, vfs_node_t *parent) {
    if (node->erofs_ino != 0) {
        return true;
    }
    if (list->count == list->cap) {
        size_t next_cap = list->cap ? list->cap * 2 : 1024;
        vfs_node_t **next_items = realloc(list->items, next_cap * sizeof(*next_items));
        if (!next_items) {
            return false;
        }
        list->items = next_items;
        vfs_node_t **next_parents = realloc(list->parents, next_cap * sizeof(*next_parents));
        if (!next_parents) {
            return false;
        }
        list->parents = next_parents;
        list->cap = next_cap;
    }
    node->erofs_ino = (uint32_t)(list->count + 1);
    node->erofs_nid = list->next_nid;
    /* Advance NID by 32-byte slots covering extended inode (64 B) + inline xattrs */
    size_t slot_bytes = (64U + (size_t)node->erofs_xattr_bytes + EROFS_ISLOT_SIZE - 1U) &
                        ~(size_t)(EROFS_ISLOT_SIZE - 1U);
    list->next_nid += (uint64_t)(slot_bytes >> EROFS_ISLOTBITS);
    list->items[list->count] = node;
    list->parents[list->count] = parent;
    list->count++;
    return true;
}

/* Iterative BFS collects every unique inode and computes exact POSIX nlinks without recursion */
static bool collect_nodes_and_nlinks(vfs_node_t *root, node_list_t *list) {
    list->next_nid = 1ULL; /* root_nid = 1 (offset 32 inside meta_blkaddr) */
    if (!node_list_add(list, root, root)) {
        return false;
    }
    for (size_t idx = 0; idx < list->count; idx++) {
        vfs_node_t *n = list->items[idx];
        if (!S_ISDIR(n->mode)) {
            continue;
        }
        n->nlink = 2; /* "." and parent's link (or ".." for root) */
        for (size_t i = 0; i < n->child_count; i++) {
            vfs_node_t *child = n->children[i].node;
            if (S_ISDIR(child->mode)) {
                n->nlink++;
            } else {
                child->nlink++;
            }
            if (!node_list_add(list, child, n)) {
                return false;
            }
        }
    }
    return true;
}

static uint8_t mode_to_erofs_ft(uint32_t mode) {
    switch (mode & S_IFMT) {
    case S_IFREG: return EROFS_FT_REG_FILE;
    case S_IFDIR: return EROFS_FT_DIR;
    case S_IFCHR: return EROFS_FT_CHRDEV;
    case S_IFBLK: return EROFS_FT_BLKDEV;
    case S_IFIFO: return EROFS_FT_FIFO;
    case S_IFSOCK: return EROFS_FT_SOCK;
    case S_IFLNK: return EROFS_FT_SYMLINK;
    default: return EROFS_FT_UNKNOWN;
    }
}

static uint32_t erofs_encode_rdev(uint32_t major, uint32_t minor) {
    return (minor & 0xFFU) | (major << 8) | ((minor & ~0xFFU) << 12);
}

static void erofs_decode_rdev(uint32_t encoded, uint32_t *major_out, uint32_t *minor_out) {
    *major_out = (encoded >> 8) & 0xFFFU;
    *minor_out = (encoded & 0xFFU) | ((encoded >> 12) & 0xFFF00U);
}

typedef struct erofs_sort_entry {
    const char *name;
    size_t name_len;
    uint64_t nid;
    uint8_t ft;
} erofs_sort_entry_t;

static int cmp_erofs_entry(const void *a, const void *b) {
    const erofs_sort_entry_t *ea = a;
    const erofs_sort_entry_t *eb = b;
    return strcmp(ea->name, eb->name);
}

/* Stream ONE directory's blocks straight to bw (zero retained directory buffers!) */
static bool stream_erofs_dir_blocks(vfs_node_t *dir, vfs_node_t *parent,
                                    buf_writer_t *bw, uint64_t *cur_blk) {
    size_t total = dir->child_count + 2;
    erofs_sort_entry_t *entries = malloc(total * sizeof(*entries));
    if (!entries) {
        return false;
    }
    entries[0] = (erofs_sort_entry_t){".", 1, dir->erofs_nid, EROFS_FT_DIR};
    entries[1] = (erofs_sort_entry_t){"..", 2, parent->erofs_nid, EROFS_FT_DIR};
    for (size_t i = 0; i < dir->child_count; i++) {
        entries[2 + i] = (erofs_sort_entry_t){
            .name = dir->children[i].name,
            .name_len = dir->children[i].name_len,
            .nid = dir->children[i].node->erofs_nid,
            .ft = mode_to_erofs_ft(dir->children[i].node->mode),
        };
    }
    qsort(entries, total, sizeof(*entries), cmp_erofs_entry);

    uint8_t blk[EROFS_BLOCK_SIZE];
    uint64_t start_blk = *cur_blk;
    size_t blk_count = 0;
    size_t i = 0;
    size_t last_blk_used = 0;

    while (i < total) {
        size_t j = i;
        size_t bytes = 0;
        while (j < total) {
            size_t add = EROFS_DIRENT_SIZE + entries[j].name_len;
            if (bytes + add > EROFS_BLOCK_SIZE) {
                break;
            }
            bytes += add;
            j++;
        }
        if (j == i) {
            free(entries);
            return false;
        }
        memset(blk, 0, sizeof(blk));
        size_t count = j - i;
        size_t nameoff = count * EROFS_DIRENT_SIZE;
        for (size_t k = 0; k < count; k++) {
            uint8_t *de = blk + k * EROFS_DIRENT_SIZE;
            put_le64(de + 0, entries[i + k].nid);
            put_le16(de + 8, (uint16_t)nameoff);
            de[10] = entries[i + k].ft;
            de[11] = 0;
            memcpy(blk + nameoff, entries[i + k].name, entries[i + k].name_len);
            nameoff += entries[i + k].name_len;
        }
        if (!bw_write(bw, blk, EROFS_BLOCK_SIZE)) {
            free(entries);
            return false;
        }
        last_blk_used = nameoff;
        blk_count++;
        i = j;
    }

    free(entries);
    if (*cur_blk + blk_count > VFS_MAX_IMAGE_BLOCKS) {
        return false;
    }
    dir->erofs_raw_blkaddr = (uint32_t)start_blk;
    dir->erofs_dir_size = (uint64_t)(blk_count - 1) * EROFS_BLOCK_SIZE + last_blk_used;
    *cur_blk += blk_count;
    return true;
}

static bool finalize_streaming_erofs(vfs_node_t *root, buf_writer_t *bw, uint64_t data_end_blk,
                                     uint8_t uuid_out[16], size_t *size_out, char **error_message) {
    node_list_t list = {0};
    if (!collect_nodes_and_nlinks(root, &list)) {
        free(list.items);
        free(list.parents);
        set_error(error_message, "out of memory collecting filesystem nodes");
        return false;
    }

    uint64_t cur_blk = data_end_blk;

    /* 1. Stream symlink payload blocks */
    for (size_t i = 0; i < list.count; i++) {
        vfs_node_t *n = list.items[i];
        if (S_ISLNK(n->mode) && n->symlink) {
            size_t slen = strlen(n->symlink);
            if (slen > 0) {
                uint64_t blks = (slen + EROFS_BLOCK_SIZE - 1) / EROFS_BLOCK_SIZE;
                if (cur_blk + blks > VFS_MAX_IMAGE_BLOCKS) {
                    free(list.items);
                    free(list.parents);
                    set_error(error_message, "EROFS image exceeds block limit");
                    return false;
                }
                n->erofs_raw_blkaddr = (uint32_t)cur_blk;
                if (!bw_write(bw, n->symlink, slen)) {
                    free(list.items);
                    free(list.parents);
                    set_error(error_message, "failed writing symlink block");
                    return false;
                }
                size_t rem = slen % EROFS_BLOCK_SIZE;
                if (rem != 0 && !bw_write_zeros(bw, EROFS_BLOCK_SIZE - rem)) {
                    free(list.items);
                    free(list.parents);
                    set_error(error_message, "failed padding symlink block");
                    return false;
                }
                cur_blk += blks;
            }
        }
    }

    /* 2. Stream directory blocks one directory at a time */
    for (size_t i = 0; i < list.count; i++) {
        vfs_node_t *n = list.items[i];
        if (S_ISDIR(n->mode)) {
            if (!stream_erofs_dir_blocks(n, list.parents[i], bw, &cur_blk)) {
                free(list.items);
                free(list.parents);
                set_error(error_message, "failed encoding EROFS directory");
                return false;
            }
        }
    }

    /* 3. Stream inode metadata table + inline xattrs starting at meta_blkaddr = cur_blk */
    if (cur_blk > UINT32_MAX) {
        free(list.items);
        free(list.parents);
        set_error(error_message, "EROFS metadata offset overflow");
        return false;
    }
    uint32_t meta_blkaddr = (uint32_t)cur_blk;
    uint64_t meta_bytes = 32ULL; /* slot 0 reserved so root_nid == 1 */
    if (!bw_write_zeros(bw, 32U)) {
        free(list.items);
        free(list.parents);
        set_error(error_message, "failed writing EROFS metadata header");
        return false;
    }

    for (size_t i = 0; i < list.count; i++) {
        vfs_node_t *n = list.items[i];
        uint8_t ino[64];
        memset(ino, 0, sizeof(ino));

        uint16_t format = (uint16_t)(EROFS_INODE_EXTENDED | (EROFS_INODE_FLAT_PLAIN << 1));
        uint64_t isize = 0;
        uint32_t iu = 0;
        if (S_ISDIR(n->mode)) {
            isize = n->erofs_dir_size;
            iu = n->erofs_raw_blkaddr;
        } else if (S_ISREG(n->mode)) {
            isize = n->size;
            iu = n->erofs_raw_blkaddr;
        } else if (S_ISLNK(n->mode)) {
            isize = n->symlink ? strlen(n->symlink) : 0;
            iu = n->erofs_raw_blkaddr;
        } else if (S_ISCHR(n->mode) || S_ISBLK(n->mode)) {
            iu = erofs_encode_rdev(n->rdev_major, n->rdev_minor);
        }

        put_le16(ino + 0x00, format);
        put_le16(ino + 0x02, n->erofs_xattr_icount);
        put_le16(ino + 0x04, (uint16_t)n->mode);
        put_le16(ino + 0x06, 0);
        put_le64(ino + 0x08, isize);
        put_le32(ino + 0x10, iu);
        put_le32(ino + 0x14, n->erofs_ino);
        put_le32(ino + 0x18, n->uid);
        put_le32(ino + 0x1C, n->gid);
        put_le64(ino + 0x20, n->mtime);
        put_le32(ino + 0x28, n->mtime_nsec);
        put_le32(ino + 0x2C, n->nlink ? n->nlink : 1);

        if (!bw_write(bw, ino, sizeof(ino))) {
            free(list.items);
            free(list.parents);
            set_error(error_message, "failed writing EROFS inode");
            return false;
        }
        size_t written_for_node = 64U;

        if (n->erofs_xattr_icount > 0) {
            uint8_t xhdr[EROFS_XATTR_IBODY_HDR_SIZE];
            memset(xhdr, 0, sizeof(xhdr));
            if (!bw_write(bw, xhdr, sizeof(xhdr))) {
                free(list.items);
                free(list.parents);
                set_error(error_message, "failed writing EROFS xattr header");
                return false;
            }
            written_for_node += sizeof(xhdr);
            for (const vfs_xattr_t *xa = n->xattrs; xa != NULL; xa = xa->next) {
                uint8_t ehdr[4] = {
                    xa->name_len,
                    xa->name_index,
                    (uint8_t)xa->value_len,
                    (uint8_t)(xa->value_len >> 8),
                };
                size_t raw_len = 4U + xa->name_len + xa->value_len;
                size_t pad_len = (4U - (raw_len & 3U)) & 3U;
                if (!bw_write(bw, ehdr, 4U) ||
                    (xa->name_len + xa->value_len > 0 &&
                     !bw_write(bw, xa->payload, (size_t)xa->name_len + xa->value_len)) ||
                    (pad_len > 0 && !bw_write_zeros(bw, pad_len))) {
                    free(list.items);
                    free(list.parents);
                    set_error(error_message, "failed writing EROFS xattr entry");
                    return false;
                }
                written_for_node += raw_len + pad_len;
            }
            size_t slot_pad = (EROFS_ISLOT_SIZE - (written_for_node & (EROFS_ISLOT_SIZE - 1U))) &
                              (EROFS_ISLOT_SIZE - 1U);
            if (slot_pad > 0) {
                if (!bw_write_zeros(bw, slot_pad)) {
                    free(list.items);
                    free(list.parents);
                    set_error(error_message, "failed padding EROFS inode slot");
                    return false;
                }
                written_for_node += slot_pad;
            }
        }
        meta_bytes += written_for_node;
    }

    /* Pad metadata region to 4 KiB block boundary, then 16 KiB PMEM alignment */
    size_t blk_rem = (size_t)(meta_bytes % EROFS_BLOCK_SIZE);
    if (blk_rem != 0) {
        if (!bw_write_zeros(bw, EROFS_BLOCK_SIZE - blk_rem)) {
            free(list.items);
            free(list.parents);
            set_error(error_message, "failed padding EROFS metadata block");
            return false;
        }
        meta_bytes += EROFS_BLOCK_SIZE - blk_rem;
    }
    cur_blk += meta_bytes / EROFS_BLOCK_SIZE;

    uint64_t raw_bytes = cur_blk * EROFS_BLOCK_SIZE;
    uint64_t padded_bytes = (raw_bytes + EROFS_ALIGN_SIZE - 1ULL) & ~(uint64_t)(EROFS_ALIGN_SIZE - 1ULL);
    if (padded_bytes > raw_bytes) {
        if (!bw_write_zeros(bw, (size_t)(padded_bytes - raw_bytes))) {
            free(list.items);
            free(list.parents);
            set_error(error_message, "failed padding EROFS tail");
            return false;
        }
    }
    if (!bw_flush(bw)) {
        free(list.items);
        free(list.parents);
        set_error(error_message, "failed flushing EROFS image");
        return false;
    }

    uint64_t total_blocks = padded_bytes / EROFS_BLOCK_SIZE;
    if (total_blocks > UINT32_MAX) {
        free(list.items);
        free(list.parents);
        set_error(error_message, "EROFS total block count overflow");
        return false;
    }

    /* 4. Write finalized EROFS superblock at Block 0 */
    arc4random_buf(uuid_out, 16);
    uuid_out[6] = (uint8_t)((uuid_out[6] & 0x0FU) | 0x40U);
    uuid_out[8] = (uint8_t)((uuid_out[8] & 0x3FU) | 0x80U);

    uint8_t block0[EROFS_BLOCK_SIZE];
    memset(block0, 0, sizeof(block0));
    uint8_t *sb = block0 + EROFS_SUPER_OFFSET;
    put_le32(sb + 0x00, EROFS_SUPER_MAGIC_V1);
    put_le32(sb + 0x04, 0);
    put_le32(sb + 0x08, 0);
    sb[0x0C] = EROFS_BLKSZBITS;
    sb[0x0D] = 0;
    put_le16(sb + 0x0E, (uint16_t)root->erofs_nid);
    put_le64(sb + 0x10, list.count);
    put_le64(sb + 0x18, (uint64_t)time(NULL));
    put_le32(sb + 0x20, 0);
    put_le32(sb + 0x24, (uint32_t)total_blocks);
    put_le32(sb + 0x28, meta_blkaddr);
    put_le32(sb + 0x2C, 0);
    memcpy(sb + 0x30, uuid_out, 16);
    memcpy(sb + 0x40, "swipterm-rootfs", 15);
    put_le32(sb + 0x50, 0);

    bool ok = pwrite_all_fd(bw->fd, block0, sizeof(block0), 0);
    free(list.items);
    free(list.parents);
    if (!ok) {
        set_error(error_message, "failed writing EROFS superblock");
        return false;
    }
    *size_out = (size_t)padded_bytes;
    return true;
}

/* =========================================================================
 * Deferred Hardlink Queue (Handles Forward & Chained Hardlinks)
 * ========================================================================= */

typedef struct deferred_hardlink {
    char *link_path;
    char *target_path;
} deferred_hardlink_t;

typedef struct deferred_hl_list {
    deferred_hardlink_t *items;
    size_t count;
    size_t cap;
} deferred_hl_list_t;

static bool deferred_hl_push(deferred_hl_list_t *list, const char *link_path, const char *target_path) {
    if (list->count == list->cap) {
        size_t next_cap = list->cap ? list->cap * 2 : 64;
        deferred_hardlink_t *next = realloc(list->items, next_cap * sizeof(*next));
        if (!next) {
            return false;
        }
        list->items = next;
        list->cap = next_cap;
    }
    char *lp = strdup(link_path);
    char *tp = strdup(target_path);
    if (!lp || !tp) {
        free(lp);
        free(tp);
        return false;
    }
    list->items[list->count++] = (deferred_hardlink_t){.link_path = lp, .target_path = tp};
    return true;
}

static void deferred_hl_free(deferred_hl_list_t *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].link_path);
        free(list->items[i].target_path);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static bool resolve_deferred_hardlinks(vfs_node_t *root, deferred_hl_list_t *list, char **error_message) {
    if (list->count == 0) {
        return true;
    }
    bool *done = calloc(list->count, sizeof(bool));
    if (!done) {
        set_error(error_message, "out of memory resolving hardlinks");
        return false;
    }
    size_t remaining = list->count;
    for (int pass = 0; pass < 8 && remaining > 0; pass++) {
        size_t progressed = 0;
        for (size_t i = 0; i < list->count; i++) {
            if (done[i]) {
                continue;
            }
            vfs_node_t *target = vfs_lookup(root, list->items[i].target_path);
            if (!target) {
                continue;
            }
            if (S_ISDIR(target->mode)) {
                free(done);
                set_error(error_message, "archive contains hardlink to directory");
                return false;
            }
            vfs_node_t *parent = NULL;
            const char *leaf = NULL;
            size_t leaf_len = 0;
            if (!vfs_resolve_parent(root, list->items[i].link_path, true, &parent, &leaf, &leaf_len) || !parent) {
                free(done);
                set_error(error_message, "invalid hardlink path in archive");
                return false;
            }
            if (!vfs_dir_set(parent, leaf, leaf_len, target)) {
                free(done);
                set_error(error_message, "out of memory linking deferred hardlink");
                return false;
            }
            done[i] = true;
            remaining--;
            progressed++;
        }
        if (progressed == 0) {
            break;
        }
    }
    free(done);
    if (remaining > 0) {
        set_error(error_message, "archive contains unresolved hardlink target");
        return false;
    }
    return true;
}

/* =========================================================================
 * Streaming Archive Importer (Peak RAM O(inodes), NOT O(rootfs_bytes)!)
 * ========================================================================= */

static bool import_archive_stream(struct archive *a, size_t total_size, const uint64_t *input_pos,
                                  const char *root_path, void *context, swipterm_progress_handler progress,
                                  char **error_message) {
    if (mkdir(root_path, 0755) != 0 && errno != EEXIST) {
        set_error(error_message, strerror(errno));
        return false;
    }

    char erofs_path[1024];
    char upper_path[1024];
    if (snprintf(erofs_path, sizeof(erofs_path), "%s/lower.erofs", root_path) >= (int)sizeof(erofs_path) ||
        snprintf(upper_path, sizeof(upper_path), "%s/upper.ext4", root_path) >= (int)sizeof(upper_path)) {
        set_error(error_message, "root path too long");
        return false;
    }

    int erofs_fd = open(erofs_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (erofs_fd < 0) {
        set_error(error_message, strerror(errno));
        return false;
    }

    buf_writer_t bw = {
        .fd = erofs_fd,
        .buf = malloc(EROFS_WRITE_BUF_SIZE),
        .cap = EROFS_WRITE_BUF_SIZE,
    };
    vfs_node_t *root = vfs_node_new(S_IFDIR | 0755);
    if (!bw.buf || !root) {
        free(bw.buf);
        vfs_node_release(root);
        close(erofs_fd);
        unlink(erofs_path);
        set_error(error_message, "out of memory");
        return false;
    }

    /* Reserve Block 0 for the superblock; regular file data streams starting at Block 1 */
    memset(bw.buf, 0, EROFS_BLOCK_SIZE);
    bw.used = EROFS_BLOCK_SIZE;
    uint64_t next_data_blk = 1ULL;
    deferred_hl_list_t deferred_hl = {0};

    size_t entry_count = 0;
    struct archive_entry *entry = NULL;
    int r = ARCHIVE_OK;

    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK || r == ARCHIVE_WARN) {
        if (++entry_count > VFS_MAX_ENTRIES) {
            set_error(error_message, "archive exceeds maximum entry limit");
            goto fail;
        }
        const char *pathname = archive_entry_pathname(entry);
        if (!pathname || pathname[0] == '\0') {
            continue;
        }

        vfs_path_stack_t parent_stack;
        vfs_node_t *parent = NULL;
        const char *leaf = NULL;
        size_t leaf_len = 0;
        if (!vfs_resolve_parent_ex(root, pathname, true, &parent_stack, &parent, &leaf, &leaf_len)) {
            set_error(error_message, "invalid path in archive");
            goto fail;
        }

        uint32_t mode = (uint32_t)archive_entry_mode(entry);
        if ((mode & S_IFMT) == 0) {
            mode |= S_IFREG;
        }

        if (!parent) {
            root->mode = (mode & 07777U) | S_IFDIR;
            root->uid = (uint32_t)archive_entry_uid(entry);
            root->gid = (uint32_t)archive_entry_gid(entry);
            root->mtime = (uint64_t)archive_entry_mtime(entry);
            root->mtime_nsec = (uint32_t)archive_entry_mtime_nsec(entry);
            if (!capture_entry_xattrs(entry, root)) {
                set_error(error_message, "out of memory reading root xattrs");
                goto fail;
            }
            continue;
        }

        /* Handle OCI/AUFS whiteout markers cleanly */
        if (leaf_len == 12 && memcmp(leaf, ".wh..wh..opq", 12) == 0) {
            vfs_dir_clear(parent);
            continue;
        }
        if (leaf_len >= 8 && memcmp(leaf, ".wh..wh.", 8) == 0) {
            continue;
        }
        if (leaf_len > 4 && memcmp(leaf, ".wh.", 4) == 0) {
            vfs_dir_remove(parent, leaf + 4, leaf_len - 4);
            continue;
        }

        const char *hardlink = archive_entry_hardlink(entry);
        if (hardlink && hardlink[0] != '\0') {
            vfs_node_t *target = vfs_lookup(root, hardlink);
            if (target) {
                if (S_ISDIR(target->mode)) {
                    set_error(error_message, "archive contains hardlink to directory");
                    goto fail;
                }
                if (!vfs_dir_set(parent, leaf, leaf_len, target)) {
                    set_error(error_message, "out of memory linking hardlink");
                    goto fail;
                }
            } else {
                if (!deferred_hl_push(&deferred_hl, pathname, hardlink)) {
                    set_error(error_message, "out of memory queuing deferred hardlink");
                    goto fail;
                }
            }
            continue;
        }

        vfs_dirent_t *existing = vfs_dir_find(parent, leaf, leaf_len);

        /* Merged-/usr case 1: directory entry arriving AFTER a symlink at the same path
         * (e.g. "./bin -> usr/bin" followed by "./bin/"): keep symlink, update target dir! */
        if (S_ISDIR(mode) && existing && S_ISLNK(existing->node->mode)) {
            vfs_path_stack_t target_st = parent_stack;
            unsigned sym_depth = 0;
            if (vfs_follow_symlink_dir(root, &target_st, existing->node->symlink, true, &sym_depth)) {
                vfs_node_t *tdir = target_st.nodes[target_st.depth];
                if (tdir->implicit_dir) {
                    tdir->mode = mode;
                    tdir->uid = (uint32_t)archive_entry_uid(entry);
                    tdir->gid = (uint32_t)archive_entry_gid(entry);
                    tdir->mtime = (uint64_t)archive_entry_mtime(entry);
                    tdir->mtime_nsec = (uint32_t)archive_entry_mtime_nsec(entry);
                    tdir->implicit_dir = false;
                }
            }
            continue;
        }

        /* Merged-/usr case 2: symlink arriving AFTER files were already extracted into an
         * implicit directory at the same path (e.g. "./bin/sh" then "./bin -> usr/bin"):
         * migrate existing directory's children into the symlink target directory! */
        if (S_ISLNK(mode) && existing && S_ISDIR(existing->node->mode)) {
            const char *sym_target = archive_entry_symlink(entry);
            if (sym_target && sym_target[0] != '\0') {
                vfs_path_stack_t target_st = parent_stack;
                unsigned sym_depth = 0;
                if (vfs_follow_symlink_dir(root, &target_st, sym_target, true, &sym_depth)) {
                    vfs_node_t *tdir = target_st.nodes[target_st.depth];
                    if (tdir != existing->node && S_ISDIR(tdir->mode)) {
                        if (!vfs_dir_merge(tdir, existing->node, 0)) {
                            set_error(error_message, "out of memory merging symlinked directory");
                            goto fail;
                        }
                    }
                }
            }
        }

        vfs_node_t *node = NULL;
        if (S_ISDIR(mode) && existing && S_ISDIR(existing->node->mode)) {
            node = vfs_node_retain(existing->node);
            node->implicit_dir = false;
        }
        if (!node) {
            node = vfs_node_new(mode);
            if (!node) {
                set_error(error_message, "out of memory");
                goto fail;
            }
        }
        node->mode = mode;
        node->uid = (uint32_t)archive_entry_uid(entry);
        node->gid = (uint32_t)archive_entry_gid(entry);
        node->mtime = (uint64_t)archive_entry_mtime(entry);
        node->mtime_nsec = (uint32_t)archive_entry_mtime_nsec(entry);

        if (!capture_entry_xattrs(entry, node)) {
            vfs_node_release(node);
            set_error(error_message, "out of memory reading xattrs");
            goto fail;
        }

        if (S_ISREG(mode)) {
            uint64_t got = 0;
            uint64_t file_start_blk = next_data_blk;
            for (;;) {
                if (bw.used == bw.cap && !bw_flush(&bw)) {
                    vfs_node_release(node);
                    set_error(error_message, strerror(errno));
                    goto fail;
                }
                la_ssize_t n = archive_read_data(a, bw.buf + bw.used, bw.cap - bw.used);
                if (n < 0) {
                    vfs_node_release(node);
                    set_error(error_message, archive_error_string(a));
                    goto fail;
                }
                if (n == 0) {
                    break;
                }
                bw.used += (size_t)n;
                got += (uint64_t)n;
            }
            if (got > 0) {
                size_t rem = (size_t)(got % EROFS_BLOCK_SIZE);
                if (rem != 0 && !bw_write_zeros(&bw, EROFS_BLOCK_SIZE - rem)) {
                    vfs_node_release(node);
                    set_error(error_message, strerror(errno));
                    goto fail;
                }
                uint64_t blks = (got + EROFS_BLOCK_SIZE - 1ULL) / EROFS_BLOCK_SIZE;
                if (next_data_blk + blks > VFS_MAX_IMAGE_BLOCKS) {
                    vfs_node_release(node);
                    set_error(error_message, "archive unpacked size exceeds EROFS limit");
                    goto fail;
                }
                node->erofs_raw_blkaddr = (uint32_t)file_start_blk;
                next_data_blk += blks;
            } else {
                node->erofs_raw_blkaddr = 0;
            }
            node->size = got;
        } else if (S_ISLNK(mode)) {
            const char *target = archive_entry_symlink(entry);
            size_t tlen = target ? strlen(target) : 0;
            if (tlen > VFS_MAX_SYMLINK_LEN) {
                vfs_node_release(node);
                set_error(error_message, "symlink target too long");
                goto fail;
            }
            free(node->symlink);
            node->symlink = strdup(target ? target : "");
            if (!node->symlink) {
                vfs_node_release(node);
                set_error(error_message, "out of memory");
                goto fail;
            }
        } else if (S_ISCHR(mode) || S_ISBLK(mode)) {
            node->rdev_major = archive_entry_rdevmajor(entry);
            node->rdev_minor = archive_entry_rdevminor(entry);
        }

        if (!vfs_dir_set(parent, leaf, leaf_len, node)) {
            vfs_node_release(node);
            set_error(error_message, "out of memory inserting entry");
            goto fail;
        }
        vfs_node_release(node);

        if (progress && total_size > 0) {
            double frac = (double)(input_pos ? *input_pos : (uint64_t)archive_filter_bytes(a, -1)) /
                          (double)total_size;
            if (frac < 0.0) {
                frac = 0.0;
            }
            if (frac > 0.9) {
                frac = 0.9;
            }
            progress(context, frac);
        }
    }

    if (r != ARCHIVE_EOF) {
        set_error(error_message, archive_error_string(a));
        goto fail;
    }
    if (entry_count == 0) {
        set_error(error_message, "archive is empty");
        goto fail;
    }

    if (!resolve_deferred_hardlinks(root, &deferred_hl, error_message)) {
        goto fail;
    }
    deferred_hl_free(&deferred_hl);

    uint8_t uuid[16];
    size_t erofs_size = 0;
    if (!finalize_streaming_erofs(root, &bw, next_data_blk, uuid, &erofs_size, error_message)) {
        goto fail;
    }

    free(bw.buf);
    bw.buf = NULL;
    vfs_node_release(root);
    root = NULL;

    if (close(erofs_fd) != 0) {
        unlink(erofs_path);
        set_error(error_message, strerror(errno));
        return false;
    }
    erofs_fd = -1;

    if (progress) {
        progress(context, 0.95);
    }

    if (swipterm_upper_create(upper_path) != 0) {
        unlink(erofs_path);
        set_error(error_message, "failed to initialize upper.ext4");
        return false;
    }
    if (!write_metadata_json(root_path, "file", uuid, erofs_size)) {
        unlink(erofs_path);
        unlink(upper_path);
        set_error(error_message, "failed to write swipterm.json");
        return false;
    }
    if (progress) {
        progress(context, 1.0);
    }
    return true;

fail:
    deferred_hl_free(&deferred_hl);
    free(bw.buf);
    vfs_node_release(root);
    if (erofs_fd >= 0) {
        close(erofs_fd);
    }
    unlink(erofs_path);
    return false;
}

/* =========================================================================
 * Native In-Process Filter Setup (No External Subprocess fork/exec on iOS!)
 * ========================================================================= */

static bool has_zstd_magic(const uint8_t *hdr, size_t len) {
    return len >= 4 && hdr[0] == 0x28 && hdr[1] == 0xB5 && hdr[2] == 0x2F && hdr[3] == 0xFD;
}

static bool setup_native_archive_reader(struct archive *a, const uint8_t *magic, size_t magic_len,
                                        char **error_message) {
    if (has_zstd_magic(magic, magic_len)) {
        if (archive_libzstd_version() != NULL) {
            if (archive_read_support_filter_zstd(a) != ARCHIVE_OK) {
                set_error(error_message, "failed to enable native zstd filter");
                return false;
            }
        } else {
            set_error(error_message,
                      "Zstandard (.tar.zst) archives are not natively supported by system libarchive; "
                      "use .tar.gz/.tar.xz/.tar.bz2 or link bundled libzstd");
            return false;
        }
    }

    /* Register ONLY verified native in-process filters so libarchive NEVER calls fork/exec on iOS */
    (void)archive_read_support_filter_none(a);
    (void)archive_read_support_filter_gzip(a);
    (void)archive_read_support_filter_bzip2(a);
    (void)archive_read_support_filter_xz(a);
    (void)archive_read_support_filter_lzma(a);
    (void)archive_read_support_filter_compress(a);
    if (archive_read_support_format_tar(a) != ARCHIVE_OK) {
        set_error(error_message, archive_error_string(a));
        return false;
    }
    return true;
}

static bool has_xz_magic(const uint8_t *hdr, size_t len) {
    static const uint8_t magic[6] = {0xFD, '7', 'z', 'X', 'Z', 0x00};
    return len >= sizeof(magic) && memcmp(hdr, magic, sizeof(magic)) == 0;
}

typedef struct xz_mt_reader {
    lzma_stream strm;
    int fd;
    bool input_eof;
    bool stream_end;
    uint8_t *in_buf;
    uint8_t *out_buf;
} xz_mt_reader_t;

static void xz_mt_reader_free(xz_mt_reader_t *r) {
    if (!r) {
        return;
    }
    lzma_end(&r->strm);
    if (r->fd >= 0) {
        close(r->fd);
    }
    free(r->in_buf);
    free(r->out_buf);
    free(r);
}

static xz_mt_reader_t *xz_mt_reader_new(int fd, const uint8_t *data, size_t size) {
    xz_mt_reader_t *r = calloc(1, sizeof(*r));
    if (!r) {
        return NULL;
    }
    r->fd = -1;
    r->out_buf = malloc(XZ_MT_BUF_SIZE);
    r->in_buf = fd >= 0 ? malloc(XZ_MT_BUF_SIZE) : NULL;
    uint32_t threads = lzma_cputhreads();
    lzma_mt mt = {
        .flags = LZMA_CONCATENATED,
        .threads = threads == 0U ? 4U : threads < 2U ? 2U : threads > 6U ? 6U : threads,
        .memlimit_threading = 128ULL << 20,
        .memlimit_stop = 512ULL << 20,
    };
    if (!r->out_buf || (fd >= 0 && !r->in_buf) || lzma_stream_decoder_mt(&r->strm, &mt) != LZMA_OK) {
        xz_mt_reader_free(r);
        return NULL;
    }
    r->fd = fd;
    if (fd < 0) {
        r->strm.next_in = data;
        r->strm.avail_in = size;
        r->input_eof = true;
    }
    return r;
}

static la_ssize_t xz_mt_read(struct archive *a, void *client, const void **buffer) {
    xz_mt_reader_t *r = client;
    r->strm.next_out = r->out_buf;
    r->strm.avail_out = XZ_MT_BUF_SIZE;
    while (!r->stream_end && r->strm.avail_out == XZ_MT_BUF_SIZE) {
        if (r->strm.avail_in == 0 && !r->input_eof) {
            ssize_t n = read(r->fd, r->in_buf, XZ_MT_BUF_SIZE);
            if (n < 0) {
                int err = errno;
                if (err == EINTR) {
                    continue;
                }
                archive_set_error(a, err, "%s", strerror(err));
                return ARCHIVE_FATAL;
            }
            r->strm.next_in = r->in_buf;
            r->strm.avail_in = (size_t)n;
            r->input_eof = n == 0;
        }
        lzma_ret ret = lzma_code(&r->strm, r->input_eof ? LZMA_FINISH : LZMA_RUN);
        if (ret == LZMA_STREAM_END) {
            r->stream_end = true;
        } else if (ret != LZMA_OK) {
            archive_set_error(a, ARCHIVE_ERRNO_MISC, "xz decompression failed (liblzma error %d)", (int)ret);
            return ARCHIVE_FATAL;
        }
    }
    *buffer = r->out_buf;
    return (la_ssize_t)(XZ_MT_BUF_SIZE - r->strm.avail_out);
}

static const uint8_t *xz_tail_bytes(int fd, const uint8_t *data, uint64_t off, size_t len, uint8_t *buf) {
    if (data) {
        return data + off;
    }
    for (size_t done = 0; done < len;) {
        ssize_t n = pread(fd, buf + done, len - done, (off_t)(off + done));
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return NULL;
        }
        done += (size_t)n;
    }
    return buf;
}

static uint64_t xz_uncompressed_size(int fd, const uint8_t *data, uint64_t size) {
    uint8_t tail_buf[XZ_TAIL_SIZE];
    size_t tail_len = size < XZ_TAIL_SIZE ? (size_t)size : XZ_TAIL_SIZE;
    const uint8_t *tail = xz_tail_bytes(fd, data, size - tail_len, tail_len, tail_buf);
    if (!tail) {
        return 0;
    }
    size_t end = tail_len;
    while (end >= 4 && get_le32(tail + end - 4) == 0) {
        end -= 4;
    }
    lzma_stream_flags flags;
    if (end < LZMA_STREAM_HEADER_SIZE ||
        lzma_stream_footer_decode(&flags, tail + end - LZMA_STREAM_HEADER_SIZE) != LZMA_OK) {
        return 0;
    }
    uint64_t padding = tail_len - end;
    uint64_t index_end = size - padding - LZMA_STREAM_HEADER_SIZE;
    if (flags.backward_size > XZ_INDEX_MAX_SIZE || flags.backward_size + LZMA_STREAM_HEADER_SIZE > index_end) {
        return 0;
    }
    size_t index_size = (size_t)flags.backward_size;
    uint8_t *index_buf = data ? NULL : malloc(index_size);
    if (!data && !index_buf) {
        return 0;
    }
    const uint8_t *index_bytes = xz_tail_bytes(fd, data, index_end - index_size, index_size, index_buf);
    uint64_t uncompressed = 0;
    lzma_index *idx = NULL;
    uint64_t memlimit = UINT64_MAX;
    size_t pos = 0;
    if (index_bytes && lzma_index_buffer_decode(&idx, &memlimit, NULL, index_bytes, &pos, index_size) == LZMA_OK) {
        if (pos == index_size && lzma_index_stream_size(idx) + padding == size) {
            uncompressed = lzma_index_uncompressed_size(idx);
        }
        lzma_index_end(idx, NULL);
    }
    free(index_buf);
    return uncompressed;
}

static bool open_archive_source(struct archive *a, xz_mt_reader_t *xz, const char *path,
                                const void *data, size_t size, const uint8_t *magic, size_t magic_len,
                                char **error_message) {
    int r;
    if (xz) {
        r = archive_read_support_filter_none(a);
        if (r == ARCHIVE_OK) {
            r = archive_read_support_format_tar(a);
        }
        if (r == ARCHIVE_OK) {
            r = archive_read_open(a, xz, NULL, xz_mt_read, NULL);
        }
    } else {
        if (!setup_native_archive_reader(a, magic, magic_len, error_message)) {
            return false;
        }
        r = path ? archive_read_open_filename(a, path, STREAM_IO_BUF_SIZE) : archive_read_open_memory(a, data, size);
    }
    if (r != ARCHIVE_OK) {
        set_error(error_message, archive_error_string(a));
        return false;
    }
    return true;
}

bool swipterm_image_import_file(const char *archive_path, const char *root_path,
                                void *context, swipterm_progress_handler progress, char **error_message) {
    if (!archive_path || !root_path) {
        set_error(error_message, "invalid arguments");
        return false;
    }
    int fd = open(archive_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        set_error(error_message, strerror(errno));
        return false;
    }
    struct stat st;
    if (fstat(fd, &st) != 0) {
        set_error(error_message, strerror(errno));
        close(fd);
        return false;
    }

    uint8_t magic[6] = {0};
    ssize_t pr = pread(fd, magic, sizeof(magic), 0);
    size_t magic_len = pr > 0 ? (size_t)pr : 0;
    xz_mt_reader_t *xz = has_xz_magic(magic, magic_len) ? xz_mt_reader_new(fd, NULL, 0) : NULL;
    uint64_t uncompressed = 0;
    if (xz) {
        uncompressed = xz_uncompressed_size(fd, NULL, (uint64_t)st.st_size);
    } else {
        close(fd);
    }

    struct archive *a = archive_read_new();
    if (!a) {
        xz_mt_reader_free(xz);
        set_error(error_message, "failed to allocate archive reader");
        return false;
    }
    bool ok = open_archive_source(a, xz, archive_path, NULL, 0, magic, magic_len, error_message) &&
              import_archive_stream(a, uncompressed > 0 ? (size_t)uncompressed : (size_t)st.st_size,
                                    xz && uncompressed == 0 ? &xz->strm.total_in : NULL, root_path,
                                    context, progress, error_message);
    archive_read_free(a);
    xz_mt_reader_free(xz);
    return ok;
}

bool swipterm_image_import_memory(const void *data, size_t size, const char *root_path,
                                  void *context, swipterm_progress_handler progress, char **error_message) {
    if (!data || size == 0 || !root_path) {
        set_error(error_message, "invalid arguments");
        return false;
    }

    const uint8_t *bytes = data;
    if (size >= EROFS_SUPER_OFFSET + 128 && get_le32(bytes + EROFS_SUPER_OFFSET) == EROFS_SUPER_MAGIC_V1) {
        const uint8_t *uuid = bytes + EROFS_SUPER_OFFSET + 0x30;
        swipterm_dax_register(uuid, data, size);
        if (mkdir(root_path, 0755) != 0 && errno != EEXIST) {
            set_error(error_message, strerror(errno));
            return false;
        }
        char upper_path[1024];
        if (snprintf(upper_path, sizeof(upper_path), "%s/upper.ext4", root_path) >= (int)sizeof(upper_path)) {
            set_error(error_message, "root path too long");
            return false;
        }
        if (swipterm_upper_create(upper_path) != 0) {
            set_error(error_message, "failed to initialize upper.ext4");
            return false;
        }
        if (!write_metadata_json(root_path, "dax", uuid, size)) {
            set_error(error_message, "failed to write swipterm.json");
            return false;
        }
        if (progress) {
            progress(context, 1.0);
        }
        return true;
    }

    size_t magic_len = size < 6 ? size : 6;
    xz_mt_reader_t *xz = has_xz_magic(bytes, magic_len) ? xz_mt_reader_new(-1, bytes, size) : NULL;
    uint64_t uncompressed = xz ? xz_uncompressed_size(-1, bytes, size) : 0;
    struct archive *a = archive_read_new();
    if (!a) {
        xz_mt_reader_free(xz);
        set_error(error_message, "failed to allocate archive reader");
        return false;
    }
    bool ok = open_archive_source(a, xz, NULL, data, size, bytes, magic_len, error_message) &&
              import_archive_stream(a, uncompressed > 0 ? (size_t)uncompressed : size,
                                    xz && uncompressed == 0 ? &xz->strm.total_in : NULL, root_path,
                                    context, progress, error_message);
    archive_read_free(a);
    xz_mt_reader_free(xz);
    return ok;
}

/* =========================================================================
 * Zero-Copy EROFS Reader & Streaming Exporter (Lower Files Stay Mmapped!)
 * ========================================================================= */

typedef struct erofs_nid_map {
    uint64_t nid;
    vfs_node_t *node;
} erofs_nid_map_t;

typedef struct erofs_reader {
    const uint8_t *base;
    size_t size;
    uint32_t block_size;
    uint32_t meta_blkaddr;
    uint64_t build_time;
    uint32_t build_time_nsec;
    erofs_nid_map_t *seen;
    size_t seen_count;
    size_t seen_cap;
} erofs_reader_t;

static vfs_node_t *erofs_find_seen(const erofs_reader_t *r, uint64_t nid) {
    for (size_t i = 0; i < r->seen_count; i++) {
        if (r->seen[i].nid == nid) {
            return r->seen[i].node;
        }
    }
    return NULL;
}

static bool erofs_record_seen(erofs_reader_t *r, uint64_t nid, vfs_node_t *node) {
    if (r->seen_count == r->seen_cap) {
        size_t next_cap = r->seen_cap ? r->seen_cap * 2 : 256;
        erofs_nid_map_t *next = realloc(r->seen, next_cap * sizeof(*next));
        if (!next) {
            return false;
        }
        r->seen = next;
        r->seen_cap = next_cap;
    }
    r->seen[r->seen_count++] = (erofs_nid_map_t){nid, node};
    return true;
}

static uint8_t *erofs_read_inode_data(const erofs_reader_t *r, uint16_t datalayout,
                                      uint32_t raw_blkaddr, size_t inline_off,
                                      uint64_t data_size) {
    if (data_size == 0) {
        return NULL;
    }
    if (data_size > 256ULL * 1024ULL * 1024ULL) {
        return NULL;
    }
    uint8_t *buf = malloc((size_t)data_size);
    if (!buf) {
        return NULL;
    }
    if (datalayout == EROFS_INODE_FLAT_PLAIN) {
        uint64_t off = (uint64_t)raw_blkaddr * r->block_size;
        if (off + data_size > r->size) {
            free(buf);
            return NULL;
        }
        memcpy(buf, r->base + off, (size_t)data_size);
        return buf;
    }
    if (datalayout == EROFS_INODE_FLAT_INLINE) {
        size_t full_bytes = (size_t)(data_size & ~(uint64_t)(r->block_size - 1));
        size_t tail_bytes = (size_t)(data_size - full_bytes);
        if (full_bytes > 0) {
            uint64_t off = (uint64_t)raw_blkaddr * r->block_size;
            if (off + full_bytes > r->size) {
                free(buf);
                return NULL;
            }
            memcpy(buf, r->base + off, full_bytes);
        }
        if (tail_bytes > 0) {
            if (inline_off + tail_bytes > r->size) {
                free(buf);
                return NULL;
            }
            memcpy(buf + full_bytes, r->base + inline_off, tail_bytes);
        }
        return buf;
    }
    free(buf);
    return NULL;
}

static const char *erofs_xattr_prefix_str(uint8_t idx) {
    switch (idx) {
    case EROFS_XATTR_INDEX_USER: return "user.";
    case EROFS_XATTR_INDEX_POSIX_ACL_ACCESS: return "system.posix_acl_access";
    case EROFS_XATTR_INDEX_POSIX_ACL_DEFAULT: return "system.posix_acl_default";
    case EROFS_XATTR_INDEX_TRUSTED: return "trusted.";
    case EROFS_XATTR_INDEX_SECURITY: return "security.";
    default: return NULL;
    }
}

static void erofs_load_inline_xattrs(const erofs_reader_t *r, size_t xattr_off, size_t xattr_size, vfs_node_t *node) {
    if (xattr_size < EROFS_XATTR_IBODY_HDR_SIZE || xattr_off + xattr_size > r->size) {
        return;
    }
    const uint8_t *base = r->base + xattr_off;
    uint8_t shared_count = base[4];
    size_t pos = EROFS_XATTR_IBODY_HDR_SIZE + (size_t)shared_count * 4U;
    while (pos + 4U <= xattr_size) {
        uint8_t nlen = base[pos + 0];
        uint8_t nidx = base[pos + 1];
        uint16_t vlen = get_le16(base + pos + 2);
        size_t raw = 4U + nlen + vlen;
        if (pos + raw > xattr_size) {
            break;
        }
        const char *pfx = erofs_xattr_prefix_str(nidx);
        if (pfx) {
            char full[300];
            size_t plen = strlen(pfx);
            if (plen + nlen < sizeof(full)) {
                memcpy(full, pfx, plen);
                if (nlen > 0) {
                    memcpy(full + plen, base + pos + 4U, nlen);
                }
                full[plen + nlen] = '\0';
                (void)vfs_node_add_xattr(node, full, base + pos + 4U + nlen, vlen);
            }
        }
        pos += (raw + 3U) & ~3U;
    }
}

static vfs_node_t *erofs_load_node(erofs_reader_t *r, uint64_t nid, int depth) {
    if (depth > (int)VFS_MAX_DEPTH) {
        return NULL;
    }
    vfs_node_t *cached = erofs_find_seen(r, nid);
    if (cached) {
        return vfs_node_retain(cached);
    }

    uint64_t iloc = (uint64_t)r->meta_blkaddr * r->block_size + (nid << EROFS_ISLOTBITS);
    if (iloc + 32 > r->size) {
        return NULL;
    }
    const uint8_t *p = r->base + iloc;
    uint16_t format = get_le16(p + 0);
    uint16_t xattr_icount = get_le16(p + 2);
    uint16_t layout = format & 1U;
    uint16_t datalayout = (format >> 1) & 7U;
    size_t xattr_size = xattr_icount ? (size_t)(xattr_icount - 1) * 4U + 12U : 0;

    uint32_t mode = 0;
    uint32_t uid = 0;
    uint32_t gid = 0;
    uint64_t isize = 0;
    uint32_t iu = 0;
    uint64_t mtime = r->build_time;
    uint32_t mtime_nsec = r->build_time_nsec;
    size_t xattr_off = 0;
    size_t inline_off = 0;

    if (layout == EROFS_INODE_EXTENDED) {
        if (iloc + 64 > r->size) {
            return NULL;
        }
        mode = get_le16(p + 0x04);
        isize = get_le64(p + 0x08);
        iu = get_le32(p + 0x10);
        uid = get_le32(p + 0x18);
        gid = get_le32(p + 0x1C);
        mtime = get_le64(p + 0x20);
        mtime_nsec = get_le32(p + 0x28);
        xattr_off = (size_t)iloc + 64;
        inline_off = xattr_off + xattr_size;
    } else {
        mode = get_le16(p + 0x04);
        isize = get_le32(p + 0x08);
        iu = get_le32(p + 0x10);
        uid = get_le16(p + 0x18);
        gid = get_le16(p + 0x1A);
        xattr_off = (size_t)iloc + 32;
        inline_off = xattr_off + xattr_size;
    }

    vfs_node_t *node = vfs_node_new(mode);
    if (!node) {
        return NULL;
    }
    node->uid = uid;
    node->gid = gid;
    node->mtime = mtime;
    node->mtime_nsec = mtime_nsec;
    if (xattr_size > 0) {
        erofs_load_inline_xattrs(r, xattr_off, xattr_size, node);
    }
    if (!erofs_record_seen(r, nid, node)) {
        vfs_node_release(node);
        return NULL;
    }

    if (S_ISREG(mode)) {
        /* Zero-copy reference into lower mmap instead of copying file bytes into RAM! */
        node->size = isize;
        node->erofs_raw_blkaddr = iu;
        node->export_inline_off = inline_off;
        node->export_datalayout = datalayout;
        node->export_from_lower = true;
    } else if (S_ISLNK(mode)) {
        uint8_t *raw = erofs_read_inode_data(r, datalayout, iu, inline_off, isize);
        node->symlink = malloc((size_t)isize + 1);
        if (!node->symlink || (isize > 0 && !raw)) {
            free(raw);
            vfs_node_release(node);
            return NULL;
        }
        if (isize > 0) {
            memcpy(node->symlink, raw, (size_t)isize);
        }
        node->symlink[isize] = '\0';
        free(raw);
    } else if (S_ISCHR(mode) || S_ISBLK(mode)) {
        erofs_decode_rdev(iu, &node->rdev_major, &node->rdev_minor);
    } else if (S_ISDIR(mode) && isize > 0) {
        uint8_t *dir_data = erofs_read_inode_data(r, datalayout, iu, inline_off, isize);
        if (!dir_data) {
            vfs_node_release(node);
            return NULL;
        }
        for (uint64_t boff = 0; boff < isize; boff += r->block_size) {
            size_t blen = (isize - boff < r->block_size) ? (size_t)(isize - boff) : r->block_size;
            if (blen < EROFS_DIRENT_SIZE) {
                break;
            }
            const uint8_t *blk = dir_data + boff;
            uint16_t nameoff0 = get_le16(blk + 8);
            if (nameoff0 < EROFS_DIRENT_SIZE || nameoff0 > blen) {
                break;
            }
            size_t count = nameoff0 / EROFS_DIRENT_SIZE;
            for (size_t i = 0; i < count; i++) {
                const uint8_t *de = blk + i * EROFS_DIRENT_SIZE;
                uint64_t child_nid = get_le64(de + 0);
                uint16_t noff = get_le16(de + 8);
                uint16_t next_off = (i + 1 < count) ? get_le16(blk + (i + 1) * EROFS_DIRENT_SIZE + 8) : (uint16_t)blen;
                if (noff > blen || next_off < noff || next_off > blen) {
                    continue;
                }
                const char *name = (const char *)(blk + noff);
                size_t nlen = (size_t)(next_off - noff);
                while (nlen > 0 && name[nlen - 1] == '\0') {
                    nlen--;
                }
                if (nlen == 0 ||
                    (nlen == 1 && name[0] == '.') ||
                    (nlen == 2 && name[0] == '.' && name[1] == '.')) {
                    continue;
                }
                vfs_node_t *child = erofs_load_node(r, child_nid, depth + 1);
                if (!child || !vfs_dir_set(node, name, nlen, child)) {
                    vfs_node_release(child);
                    free(dir_data);
                    vfs_node_release(node);
                    return NULL;
                }
                vfs_node_release(child);
            }
        }
        free(dir_data);
    }

    return node;
}

static vfs_node_t *load_erofs_tree(const uint8_t *base, size_t size, uint32_t *block_size_out, char **error_message) {
    if (size < EROFS_SUPER_OFFSET + 128 || get_le32(base + EROFS_SUPER_OFFSET) != EROFS_SUPER_MAGIC_V1) {
        set_error(error_message, "invalid EROFS image");
        return NULL;
    }
    const uint8_t *sb = base + EROFS_SUPER_OFFSET;
    uint8_t blkszbits = sb[0x0C];
    if (blkszbits < 9 || blkszbits > 16) {
        set_error(error_message, "unsupported EROFS block size");
        return NULL;
    }
    erofs_reader_t r = {
        .base = base,
        .size = size,
        .block_size = 1U << blkszbits,
        .meta_blkaddr = get_le32(sb + 0x28),
        .build_time = get_le64(sb + 0x18),
        .build_time_nsec = get_le32(sb + 0x20),
    };
    *block_size_out = r.block_size;
    uint16_t root_nid = get_le16(sb + 0x0E);
    vfs_node_t *root = erofs_load_node(&r, root_nid, 0);
    free(r.seen);
    if (!root) {
        set_error(error_message, "failed to read EROFS root");
    }
    return root;
}

typedef struct upper_apply {
    struct upper_link {
        uint32_t ino;
        vfs_node_t *node;
    } *links;
    size_t link_cap;
    size_t link_count;
} upper_apply_t;

static vfs_node_t **upper_link_slot(upper_apply_t *a, uint32_t ino) {
    if ((a->link_count + 1) * 2 > a->link_cap) {
        size_t cap = a->link_cap ? a->link_cap * 2 : 64;
        struct upper_link *links = calloc(cap, sizeof(*links));
        if (!links) {
            return NULL;
        }
        for (size_t i = 0; i < a->link_cap; i++) {
            if (a->links[i].ino != 0) {
                size_t pos = (a->links[i].ino * 2654435761U) & (cap - 1);
                while (links[pos].ino != 0) {
                    pos = (pos + 1) & (cap - 1);
                }
                links[pos] = a->links[i];
            }
        }
        free(a->links);
        a->links = links;
        a->link_cap = cap;
    }
    size_t pos = (ino * 2654435761U) & (a->link_cap - 1);
    while (a->links[pos].ino != 0 && a->links[pos].ino != ino) {
        pos = (pos + 1) & (a->link_cap - 1);
    }
    if (a->links[pos].ino == 0) {
        a->links[pos].ino = ino;
        a->link_count++;
    }
    return &a->links[pos].node;
}

static void upper_apply_free(upper_apply_t *a) {
    for (size_t i = 0; i < a->link_cap; i++) {
        vfs_node_release(a->links[i].node);
    }
    free(a->links);
}

static bool vfs_node_set_upper_xattrs(vfs_node_t *node, const ext4_entry_info_t *entry) {
    vfs_node_free_xattrs(node);
    for (size_t i = entry->xattr_count; i > 0; i--) {
        const ext4_xattr_info_t *x = &entry->xattrs[i - 1];
        uint8_t idx = 0;
        const char *suffix = NULL;
        if (!map_xattr_prefix(x->name, &idx, &suffix)) {
            continue;
        }
        size_t slen = strlen(suffix);
        if (slen > VFS_MAX_NAME_LEN || x->size > UINT16_MAX) {
            return false;
        }
        vfs_xattr_t *xa = vfs_xattr_new(idx, suffix, slen, x->value, x->size);
        if (!xa) {
            return false;
        }
        xa->next = node->xattrs;
        node->xattrs = xa;
    }
    return true;
}

static void vfs_node_set_upper_attrs(vfs_node_t *node, const ext4_entry_info_t *entry) {
    node->mode = entry->mode;
    node->uid = entry->uid;
    node->gid = entry->gid;
    node->mtime = (uint64_t)entry->mtime;
    node->mtime_nsec = entry->mtime_nsec;
}

static int apply_upper_entry(void *ctx, void *parent_ctx, const ext4_entry_info_t *entry, void **child_out) {
    upper_apply_t *apply = ctx;
    vfs_node_t *parent = parent_ctx;
    const char *leaf = entry->name;
    size_t leaf_len = entry->name_len;
    if (leaf_len == 0 || leaf_len > VFS_MAX_NAME_LEN) {
        return -LINUX_EIO;
    }

    if (S_ISCHR(entry->mode) && entry->rdev == 0) {
        vfs_dir_remove(parent, leaf, leaf_len);
        return 0;
    }

    if (S_ISDIR(entry->mode)) {
        vfs_dirent_t *existing = vfs_dir_find(parent, leaf, leaf_len);
        vfs_node_t *dir = NULL;
        if (existing && S_ISDIR(existing->node->mode)) {
            dir = existing->node;
            if (entry->opaque_dir) {
                vfs_dir_clear(dir);
            }
        } else {
            dir = vfs_node_new(entry->mode);
            if (!dir || !vfs_dir_set(parent, leaf, leaf_len, dir)) {
                vfs_node_release(dir);
                return -LINUX_ENOMEM;
            }
            vfs_node_release(dir);
        }
        dir->implicit_dir = false;
        vfs_node_set_upper_attrs(dir, entry);
        if (!vfs_node_set_upper_xattrs(dir, entry)) {
            return -LINUX_ENOMEM;
        }
        *child_out = dir;
        return 0;
    }

    vfs_node_t **link = NULL;
    if (S_ISREG(entry->mode) && entry->nlink > 1) {
        link = upper_link_slot(apply, entry->ino);
        if (!link) {
            return -LINUX_ENOMEM;
        }
        if (*link) {
            return vfs_dir_set(parent, leaf, leaf_len, *link) ? 0 : -LINUX_ENOMEM;
        }
    }

    vfs_node_t *node = vfs_node_new(entry->mode);
    if (!node) {
        return -LINUX_ENOMEM;
    }
    vfs_node_set_upper_attrs(node, entry);
    bool ok = vfs_node_set_upper_xattrs(node, entry);
    if (S_ISREG(entry->mode)) {
        node->size = entry->size;
        node->upper_ino = entry->ino;
        node->export_from_upper = true;
    } else if (S_ISLNK(entry->mode)) {
        node->symlink = strdup(entry->symlink_target ? entry->symlink_target : "");
        ok = ok && node->symlink;
    } else if (S_ISCHR(entry->mode) || S_ISBLK(entry->mode)) {
        node->rdev_major = (uint32_t)(entry->rdev >> 32);
        node->rdev_minor = (uint32_t)(entry->rdev & 0xFFFFFFFFU);
    }
    ok = ok && vfs_dir_set(parent, leaf, leaf_len, node);
    if (ok && link) {
        *link = vfs_node_retain(node);
    }
    vfs_node_release(node);
    return ok ? 0 : -LINUX_ENOMEM;
}

typedef struct tree_frame {
    vfs_node_t *dir;
    char *path;
    size_t next;
} tree_frame_t;

static bool tree_push(tree_frame_t **frames, size_t *depth, size_t *cap, vfs_node_t *dir, char *path) {
    if (*depth == *cap) {
        size_t next_cap = *cap ? *cap * 2 : 32;
        tree_frame_t *next = realloc(*frames, next_cap * sizeof(*next));
        if (!next) {
            return false;
        }
        *frames = next;
        *cap = next_cap;
    }
    (*frames)[(*depth)++] = (tree_frame_t){.dir = dir, .path = path};
    return true;
}

static bool count_tree_entries(vfs_node_t *root, size_t *count_out) {
    tree_frame_t *frames = NULL;
    size_t depth = 0;
    size_t cap = 0;
    size_t count = 1;
    bool ok = !S_ISDIR(root->mode) || tree_push(&frames, &depth, &cap, root, NULL);
    while (ok && depth > 0) {
        tree_frame_t *frame = &frames[depth - 1];
        if (frame->next == frame->dir->child_count) {
            depth--;
            continue;
        }
        vfs_node_t *child = frame->dir->children[frame->next++].node;
        count++;
        if (S_ISDIR(child->mode)) {
            ok = tree_push(&frames, &depth, &cap, child, NULL);
        }
    }
    free(frames);
    *count_out = count;
    return ok;
}

typedef struct export_state {
    struct archive *a;
    const uint8_t *lower_base;
    size_t lower_size;
    uint32_t lower_block_size;
    swipterm_upper_t *upper;
    size_t total;
    size_t done;
    void *ctx;
    swipterm_progress_handler progress;
} export_state_t;

static bool write_archive_bytes(struct archive *a, const uint8_t *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        la_ssize_t wr = archive_write_data(a, buf + off, len - off);
        if (wr < 0) {
            return false;
        }
        off += (size_t)wr;
    }
    return true;
}

static int write_archive_chunk(void *ctx, const void *data, size_t len) {
    return write_archive_bytes(ctx, data, len) ? 0 : -LINUX_EIO;
}

static bool export_lower_data(export_state_t *st, const vfs_node_t *node) {
    if (node->export_datalayout == EROFS_INODE_FLAT_PLAIN) {
        uint64_t off = (uint64_t)node->erofs_raw_blkaddr * st->lower_block_size;
        return off + node->size <= st->lower_size &&
               write_archive_bytes(st->a, st->lower_base + off, (size_t)node->size);
    }
    if (node->export_datalayout != EROFS_INODE_FLAT_INLINE) {
        return false;
    }
    size_t full_bytes = (size_t)(node->size & ~(uint64_t)(st->lower_block_size - 1));
    size_t tail_bytes = (size_t)(node->size - full_bytes);
    if (full_bytes > 0) {
        uint64_t off = (uint64_t)node->erofs_raw_blkaddr * st->lower_block_size;
        if (off + full_bytes > st->lower_size || !write_archive_bytes(st->a, st->lower_base + off, full_bytes)) {
            return false;
        }
    }
    return tail_bytes == 0 ||
           (node->export_inline_off + tail_bytes <= st->lower_size &&
            write_archive_bytes(st->a, st->lower_base + node->export_inline_off, tail_bytes));
}

static bool export_entry(export_state_t *st, vfs_node_t *node, const char *path) {
    struct archive_entry *entry = archive_entry_new();
    if (!entry) {
        return false;
    }
    archive_entry_set_pathname(entry, path);
    archive_entry_set_mode(entry, (mode_t)node->mode);
    archive_entry_set_uid(entry, (la_int64_t)node->uid);
    archive_entry_set_gid(entry, (la_int64_t)node->gid);
    archive_entry_set_mtime(entry, (time_t)node->mtime, (long)node->mtime_nsec);

    char full[VFS_MAX_NAME_LEN + 32];
    for (const vfs_xattr_t *xa = node->xattrs; xa != NULL; xa = xa->next) {
        const char *pfx = erofs_xattr_prefix_str(xa->name_index);
        size_t plen = pfx ? strlen(pfx) : 0;
        if (pfx && plen + xa->name_len < sizeof(full)) {
            memcpy(full, pfx, plen);
            if (xa->name_len > 0) {
                memcpy(full + plen, xa->payload, xa->name_len);
            }
            full[plen + xa->name_len] = '\0';
            archive_entry_xattr_add_entry(entry, full, xa->payload + xa->name_len, xa->value_len);
        }
    }

    bool is_hardlink = false;
    if (!S_ISDIR(node->mode) && node->exported_path != NULL) {
        archive_entry_set_hardlink(entry, node->exported_path);
        archive_entry_set_size(entry, 0);
        is_hardlink = true;
    } else {
        if (!S_ISDIR(node->mode)) {
            node->exported_path = strdup(path);
            if (!node->exported_path) {
                archive_entry_free(entry);
                return false;
            }
        }
        if (S_ISREG(node->mode)) {
            archive_entry_set_size(entry, (la_int64_t)node->size);
        } else if (S_ISLNK(node->mode)) {
            archive_entry_set_symlink(entry, node->symlink ? node->symlink : "");
        } else if (S_ISCHR(node->mode) || S_ISBLK(node->mode)) {
            archive_entry_set_rdevmajor(entry, node->rdev_major);
            archive_entry_set_rdevminor(entry, node->rdev_minor);
        }
    }

    int r = archive_write_header(st->a, entry);
    archive_entry_free(entry);
    if (r != ARCHIVE_OK && r != ARCHIVE_WARN) {
        return false;
    }

    if (!is_hardlink && S_ISREG(node->mode) && node->size > 0) {
        bool ok = false;
        if (node->export_from_upper) {
            ok = st->upper && swipterm_upper_read(st->upper, node->upper_ino, write_archive_chunk, st->a) == 0;
        } else if (node->export_from_lower) {
            ok = export_lower_data(st, node);
        }
        if (!ok) {
            return false;
        }
    }

    st->done++;
    if (st->progress && st->total > 0) {
        st->progress(st->ctx, (double)st->done / (double)st->total);
    }
    return true;
}

static char *join_archive_path(const char *parent, const char *name, size_t name_len) {
    bool is_dot = parent[0] == '.' && parent[1] == '\0';
    size_t plen = is_dot ? 0 : strlen(parent) + 1;
    char *path = malloc(plen + name_len + 1);
    if (!path) {
        return NULL;
    }
    if (!is_dot) {
        memcpy(path, parent, plen - 1);
        path[plen - 1] = '/';
    }
    memcpy(path + plen, name, name_len);
    path[plen + name_len] = '\0';
    return path;
}

static bool export_tree(export_state_t *st, vfs_node_t *root) {
    char *root_path = strdup(".");
    tree_frame_t *frames = NULL;
    size_t depth = 0;
    size_t cap = 0;
    bool ok = root_path && export_entry(st, root, root_path);
    if (ok && S_ISDIR(root->mode)) {
        ok = tree_push(&frames, &depth, &cap, root, root_path);
        if (ok) {
            root_path = NULL;
        }
    }
    free(root_path);
    while (ok && depth > 0) {
        tree_frame_t *frame = &frames[depth - 1];
        if (frame->next == frame->dir->child_count) {
            free(frame->path);
            depth--;
            continue;
        }
        const vfs_dirent_t *child = &frame->dir->children[frame->next++];
        char *path = join_archive_path(frame->path, child->name, child->name_len);
        ok = path && export_entry(st, child->node, path);
        if (ok && S_ISDIR(child->node->mode)) {
            ok = tree_push(&frames, &depth, &cap, child->node, path);
            if (ok) {
                path = NULL;
            }
        }
        free(path);
    }
    while (depth > 0) {
        free(frames[--depth].path);
    }
    free(frames);
    return ok;
}

static bool export_stopped_root(const char *root_path, const char *archive_path,
                                void *context, swipterm_progress_handler progress, char **error_message) {
    char lower_path[1024];
    char upper_path[1024];
    if (snprintf(lower_path, sizeof(lower_path), "%s/lower.erofs", root_path) >= (int)sizeof(lower_path) ||
        snprintf(upper_path, sizeof(upper_path), "%s/upper.ext4", root_path) >= (int)sizeof(upper_path)) {
        set_error(error_message, "path too long");
        return false;
    }

    const void *lower_base = NULL;
    size_t lower_size = 0;
    bool lower_mapped = false;

    int lower_fd = open(lower_path, O_RDONLY);
    if (lower_fd >= 0) {
        struct stat st;
        if (fstat(lower_fd, &st) == 0 && st.st_size > 0) {
            void *p = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, lower_fd, 0);
            if (p != MAP_FAILED) {
                lower_base = p;
                lower_size = (size_t)st.st_size;
                lower_mapped = true;
            }
        }
        close(lower_fd);
    }
    if (!lower_base) {
        uint8_t uuid[16];
        if (!swipterm_root_read_dax_uuid(root_path, uuid) ||
            !swipterm_dax_lookup(uuid, &lower_base, &lower_size)) {
            set_error(error_message, "unable to locate lower EROFS image");
            return false;
        }
    }

    bool ok = false;
    struct archive *a = NULL;
    swipterm_upper_t *upper = NULL;
    upper_apply_t apply = {0};
    uint32_t lower_block_size = EROFS_BLOCK_SIZE;
    vfs_node_t *root = load_erofs_tree(lower_base, lower_size, &lower_block_size, error_message);
    if (!root) {
        goto out;
    }

    int rc = swipterm_upper_open(upper_path, &upper);
    if (rc == 0 && upper) {
        rc = swipterm_upper_walk(upper, root, apply_upper_entry, &apply);
    }
    if (rc < 0) {
        set_error(error_message, "failed to read upper.ext4 overlay");
        goto out;
    }

    size_t total = 0;
    if (!count_tree_entries(root, &total)) {
        set_error(error_message, "out of memory counting filesystem nodes");
        goto out;
    }

    a = archive_write_new();
    if (!a) {
        set_error(error_message, "failed to allocate archive writer");
        goto out;
    }
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);
    if (archive_write_open_filename(a, archive_path) != ARCHIVE_OK) {
        set_error(error_message, archive_error_string(a));
        goto out;
    }

    export_state_t st = {
        .a = a,
        .lower_base = lower_base,
        .lower_size = lower_size,
        .lower_block_size = lower_block_size,
        .upper = upper,
        .total = total,
        .ctx = context,
        .progress = progress,
    };
    ok = export_tree(&st, root);
    if (!ok) {
        const char *message = archive_error_string(a);
        set_error(error_message, message ? message : "failed to write archive entry");
    }
    if (archive_write_close(a) != ARCHIVE_OK && ok) {
        set_error(error_message, archive_error_string(a));
        ok = false;
    }
    if (!ok) {
        unlink(archive_path);
    }

out:
    if (a) {
        archive_write_free(a);
    }
    upper_apply_free(&apply);
    swipterm_upper_close(upper);
    vfs_node_release(root);
    if (lower_mapped) {
        munmap((void *)(uintptr_t)lower_base, lower_size);
    }
    return ok;
}

bool swipterm_image_export(const char *root_path, const char *archive_path,
                           void *context, swipterm_progress_handler progress, char **error_message) {
    if (!root_path || !archive_path) {
        set_error(error_message, "invalid arguments");
        return false;
    }
    int root_fd = open(root_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (root_fd < 0) {
        set_error(error_message, strerror(errno));
        return false;
    }
    if (flock(root_fd, LOCK_SH | LOCK_NB) != 0) {
        set_error(error_message, errno == EWOULDBLOCK ? "the machine using this root is running" : strerror(errno));
        close(root_fd);
        return false;
    }
    bool ok = export_stopped_root(root_path, archive_path, context, progress, error_message);
    close(root_fd);
    return ok;
}
