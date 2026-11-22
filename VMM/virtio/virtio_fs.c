#include "virtio_internal.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <unistd.h>

enum {
    VIRTIO_FS_VQ_HIPRIO = 0,
    VIRTIO_FS_VQ_REQUEST = 1,
    VIRTIO_FS_HIPRIO_SIZE = 64,
    VIRTIO_FS_REQ_SIZE = 256,
    VIRTIO_FS_MAX_IOV = 128,
    VIRTIO_FS_TAG_LEN = 36,

    FUSE_KERNEL_VERSION = 7,
    FUSE_KERNEL_MINOR_VERSION = 31,
    FUSE_ROOT_ID = 1,
    FUSE_MAX_WRITE = 131072,
    FUSE_MAX_PAGES = 32,
};

enum : uint32_t {
    FUSE_LOOKUP = 1,
    FUSE_FORGET = 2,
    FUSE_GETATTR = 3,
    FUSE_SETATTR = 4,
    FUSE_READLINK = 5,
    FUSE_SYMLINK = 6,
    FUSE_MKNOD = 8,
    FUSE_MKDIR = 9,
    FUSE_UNLINK = 10,
    FUSE_RMDIR = 11,
    FUSE_RENAME = 12,
    FUSE_LINK = 13,
    FUSE_OPEN = 14,
    FUSE_READ = 15,
    FUSE_WRITE = 16,
    FUSE_STATFS = 17,
    FUSE_RELEASE = 18,
    FUSE_FSYNC = 20,
    FUSE_SETXATTR = 21,
    FUSE_GETXATTR = 22,
    FUSE_LISTXATTR = 23,
    FUSE_REMOVEXATTR = 24,
    FUSE_FLUSH = 25,
    FUSE_INIT = 26,
    FUSE_OPENDIR = 27,
    FUSE_READDIR = 28,
    FUSE_RELEASEDIR = 29,
    FUSE_FSYNCDIR = 30,
    FUSE_ACCESS = 34,
    FUSE_CREATE = 35,
    FUSE_INTERRUPT = 36,
    FUSE_DESTROY = 38,
    FUSE_BATCH_FORGET = 42,
    FUSE_FALLOCATE = 43,
    FUSE_READDIRPLUS = 44,
    FUSE_RENAME2 = 45,
    FUSE_LSEEK = 46,
};

enum : uint32_t {
    FUSE_ASYNC_READ = 1U << 0,
    FUSE_ATOMIC_O_TRUNC = 1U << 3,
    FUSE_EXPORT_SUPPORT = 1U << 4,
    FUSE_BIG_WRITES = 1U << 5,
    FUSE_DONT_MASK = 1U << 6,
    FUSE_DO_READDIRPLUS = 1U << 13,
    FUSE_READDIRPLUS_AUTO = 1U << 14,
    FUSE_PARALLEL_DIROPS = 1U << 18,
    FUSE_MAX_PAGES_FLAG = 1U << 22,
};

enum : uint32_t {
    FATTR_MODE = 1U << 0,
    FATTR_UID = 1U << 1,
    FATTR_GID = 1U << 2,
    FATTR_SIZE = 1U << 3,
    FATTR_ATIME = 1U << 4,
    FATTR_MTIME = 1U << 5,
    FATTR_FH = 1U << 6,
    FATTR_ATIME_NOW = 1U << 7,
    FATTR_MTIME_NOW = 1U << 8,
};

enum : uint32_t {
    FUSE_GETATTR_FH = 1U << 0,
    LINUX_O_ACCMODE = 3U,
    LINUX_O_RDONLY = 0U,
    LINUX_O_WRONLY = 1U,
    LINUX_O_RDWR = 2U,
    LINUX_O_CREAT = 0100U,
    LINUX_O_EXCL = 0200U,
    LINUX_O_TRUNC = 01000U,
    LINUX_O_APPEND = 02000U,
    LINUX_O_NONBLOCK = 04000U,
    LINUX_O_DIRECTORY = 040000U,
    LINUX_O_NOFOLLOW = 0100000U,
    LINUX_RENAME_NOREPLACE = 1U << 0,
    LINUX_RENAME_EXCHANGE = 1U << 1,
};

enum : int32_t {
    LINUX_EPERM = 1,
    LINUX_ENOENT = 2,
    LINUX_EINTR = 4,
    LINUX_EIO = 5,
    LINUX_ENXIO = 6,
    LINUX_EBADF = 9,
    LINUX_EAGAIN = 11,
    LINUX_ENOMEM = 12,
    LINUX_EACCES = 13,
    LINUX_EFAULT = 14,
    LINUX_EBUSY = 16,
    LINUX_EEXIST = 17,
    LINUX_EXDEV = 18,
    LINUX_ENODEV = 19,
    LINUX_ENOTDIR = 20,
    LINUX_EISDIR = 21,
    LINUX_EINVAL = 22,
    LINUX_ENFILE = 23,
    LINUX_EMFILE = 24,
    LINUX_EFBIG = 27,
    LINUX_ENOSPC = 28,
    LINUX_ESPIPE = 29,
    LINUX_EROFS = 30,
    LINUX_EMLINK = 31,
    LINUX_EPIPE = 32,
    LINUX_ERANGE = 34,
    LINUX_EDEADLK = 35,
    LINUX_ENAMETOOLONG = 36,
    LINUX_ENOSYS = 38,
    LINUX_ENOTEMPTY = 39,
    LINUX_ELOOP = 40,
    LINUX_ENODATA = 61,
    LINUX_EOVERFLOW = 75,
    LINUX_EOPNOTSUPP = 95,
    LINUX_ESTALE = 116,
    LINUX_EDQUOT = 122,
};

typedef struct __attribute__((packed)) virtio_fs_config {
    char tag[VIRTIO_FS_TAG_LEN];
    uint32_t num_request_queues;
} virtio_fs_config_t;

typedef struct fuse_in_header {
    uint32_t len;
    uint32_t opcode;
    uint64_t unique;
    uint64_t nodeid;
    uint32_t uid;
    uint32_t gid;
    uint32_t pid;
    uint32_t padding;
} fuse_in_header_t;

typedef struct fuse_out_header {
    uint32_t len;
    int32_t error;
    uint64_t unique;
} fuse_out_header_t;

typedef struct fuse_attr {
    uint64_t ino;
    uint64_t size;
    uint64_t blocks;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t atimensec;
    uint32_t mtimensec;
    uint32_t ctimensec;
    uint32_t mode;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint32_t rdev;
    uint32_t blksize;
    uint32_t padding;
} fuse_attr_t;

typedef struct fuse_kstatfs {
    uint64_t blocks;
    uint64_t bfree;
    uint64_t bavail;
    uint64_t files;
    uint64_t ffree;
    uint32_t bsize;
    uint32_t namelen;
    uint32_t frsize;
    uint32_t padding;
    uint32_t spare[6];
} fuse_kstatfs_t;

typedef struct fuse_entry_out {
    uint64_t nodeid;
    uint64_t generation;
    uint64_t entry_valid;
    uint64_t attr_valid;
    uint32_t entry_valid_nsec;
    uint32_t attr_valid_nsec;
    fuse_attr_t attr;
} fuse_entry_out_t;

typedef struct fuse_forget_in {
    uint64_t nlookup;
} fuse_forget_in_t;

typedef struct fuse_batch_forget_in {
    uint32_t count;
    uint32_t dummy;
} fuse_batch_forget_in_t;

typedef struct fuse_forget_one {
    uint64_t nodeid;
    uint64_t nlookup;
} fuse_forget_one_t;

typedef struct fuse_getattr_in {
    uint32_t getattr_flags;
    uint32_t dummy;
    uint64_t fh;
} fuse_getattr_in_t;

typedef struct fuse_attr_out {
    uint64_t attr_valid;
    uint32_t attr_valid_nsec;
    uint32_t dummy;
    fuse_attr_t attr;
} fuse_attr_out_t;

typedef struct fuse_mknod_in {
    uint32_t mode;
    uint32_t rdev;
    uint32_t umask;
    uint32_t padding;
} fuse_mknod_in_t;

typedef struct fuse_mkdir_in {
    uint32_t mode;
    uint32_t umask;
} fuse_mkdir_in_t;

typedef struct fuse_rename_in {
    uint64_t newdir;
} fuse_rename_in_t;

typedef struct fuse_rename2_in {
    uint64_t newdir;
    uint32_t flags;
    uint32_t padding;
} fuse_rename2_in_t;

typedef struct fuse_link_in {
    uint64_t oldnodeid;
} fuse_link_in_t;

typedef struct fuse_setattr_in {
    uint32_t valid;
    uint32_t padding;
    uint64_t fh;
    uint64_t size;
    uint64_t lock_owner;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t atimensec;
    uint32_t mtimensec;
    uint32_t ctimensec;
    uint32_t mode;
    uint32_t unused4;
    uint32_t uid;
    uint32_t gid;
    uint32_t unused5;
} fuse_setattr_in_t;

typedef struct fuse_open_in {
    uint32_t flags;
    uint32_t open_flags;
} fuse_open_in_t;

typedef struct fuse_create_in {
    uint32_t flags;
    uint32_t mode;
    uint32_t umask;
    uint32_t open_flags;
} fuse_create_in_t;

typedef struct fuse_open_out {
    uint64_t fh;
    uint32_t open_flags;
    uint32_t padding;
} fuse_open_out_t;

typedef struct fuse_release_in {
    uint64_t fh;
    uint32_t flags;
    uint32_t release_flags;
    uint64_t lock_owner;
} fuse_release_in_t;

typedef struct fuse_read_in {
    uint64_t fh;
    uint64_t offset;
    uint32_t size;
    uint32_t read_flags;
    uint64_t lock_owner;
    uint32_t flags;
    uint32_t padding;
} fuse_read_in_t;

typedef struct fuse_write_in {
    uint64_t fh;
    uint64_t offset;
    uint32_t size;
    uint32_t write_flags;
    uint64_t lock_owner;
    uint32_t flags;
    uint32_t padding;
} fuse_write_in_t;

typedef struct fuse_write_out {
    uint32_t size;
    uint32_t padding;
} fuse_write_out_t;

typedef struct fuse_fsync_in {
    uint64_t fh;
    uint32_t fsync_flags;
    uint32_t padding;
} fuse_fsync_in_t;

typedef struct fuse_access_in {
    uint32_t mask;
    uint32_t padding;
} fuse_access_in_t;

typedef struct fuse_init_in {
    uint32_t major;
    uint32_t minor;
    uint32_t max_readahead;
    uint32_t flags;
} fuse_init_in_t;

typedef struct fuse_init_out {
    uint32_t major;
    uint32_t minor;
    uint32_t max_readahead;
    uint32_t flags;
    uint16_t max_background;
    uint16_t congestion_threshold;
    uint32_t max_write;
    uint32_t time_gran;
    uint16_t max_pages;
    uint16_t map_alignment;
    uint32_t unused[8];
} fuse_init_out_t;

typedef struct fuse_dirent {
    uint64_t ino;
    uint64_t off;
    uint32_t namelen;
    uint32_t type;
} fuse_dirent_t;

typedef struct fuse_direntplus {
    fuse_entry_out_t entry_out;
    fuse_dirent_t dirent;
} fuse_direntplus_t;

typedef struct fuse_lseek_in {
    uint64_t fh;
    uint64_t offset;
    uint32_t whence;
    uint32_t padding;
} fuse_lseek_in_t;

typedef struct fuse_lseek_out {
    uint64_t offset;
} fuse_lseek_out_t;

static_assert(sizeof(fuse_in_header_t) == 40);
static_assert(sizeof(fuse_out_header_t) == 16);
static_assert(sizeof(fuse_attr_t) == 88);
static_assert(sizeof(fuse_entry_out_t) == 128);
static_assert(sizeof(fuse_attr_out_t) == 104);
static_assert(sizeof(fuse_init_out_t) == 64);
static_assert(sizeof(fuse_dirent_t) == 24);
static_assert(sizeof(fuse_direntplus_t) == 152);

typedef struct fs_inode {
    uint64_t nodeid;
    uint64_t generation;
    dev_t dev;
    ino_t ino;
    int fd;
    uint64_t nlookup;
    bool in_use;
} fs_inode_t;

typedef struct fs_dir_entry {
    uint64_t ino;
    uint32_t type;
    char *name;
} fs_dir_entry_t;

typedef struct fs_handle {
    uint64_t fh;
    uint64_t nodeid;
    int fd;
    bool is_dir;
    bool in_use;
    fs_dir_entry_t *entries;
    size_t entry_count;
} fs_handle_t;

typedef struct virtio_fs {
    virtio_dev_t *dev;
    virtio_fs_config_t config;
    bool read_only;
    pthread_t worker;
    bool worker_started;
    pthread_mutex_t lock;
    pthread_cond_t work_cond;
    pthread_cond_t idle_cond;
    bool pending;
    bool busy;
    bool stop;

    fs_inode_t *inodes;
    size_t inode_cap;
    uint64_t next_nodeid;
    uint64_t generation;

    fs_handle_t *handles;
    size_t handle_cap;
    uint64_t next_fh;
} virtio_fs_t;

static int32_t darwin_to_linux_errno(int err) {
    switch (err) {
    case EPERM: return LINUX_EPERM;
    case ENOENT: return LINUX_ENOENT;
    case EINTR: return LINUX_EINTR;
    case EIO: return LINUX_EIO;
    case ENXIO: return LINUX_ENXIO;
    case EBADF: return LINUX_EBADF;
    case EAGAIN: return LINUX_EAGAIN;
    case ENOMEM: return LINUX_ENOMEM;
    case EACCES: return LINUX_EACCES;
    case EFAULT: return LINUX_EFAULT;
    case EBUSY: return LINUX_EBUSY;
    case EEXIST: return LINUX_EEXIST;
    case EXDEV: return LINUX_EXDEV;
    case ENODEV: return LINUX_ENODEV;
    case ENOTDIR: return LINUX_ENOTDIR;
    case EISDIR: return LINUX_EISDIR;
    case EINVAL: return LINUX_EINVAL;
    case ENFILE: return LINUX_ENFILE;
    case EMFILE: return LINUX_EMFILE;
    case EFBIG: return LINUX_EFBIG;
    case ENOSPC: return LINUX_ENOSPC;
    case ESPIPE: return LINUX_ESPIPE;
    case EROFS: return LINUX_EROFS;
    case EMLINK: return LINUX_EMLINK;
    case EPIPE: return LINUX_EPIPE;
    case ERANGE: return LINUX_ERANGE;
    case EDEADLK: return LINUX_EDEADLK;
    case ENAMETOOLONG: return LINUX_ENAMETOOLONG;
    case ENOSYS: return LINUX_ENOSYS;
    case ENOTEMPTY: return LINUX_ENOTEMPTY;
    case ELOOP: return LINUX_ELOOP;
    case ENODATA: return LINUX_ENODATA;
    case EOVERFLOW: return LINUX_EOVERFLOW;
    case EOPNOTSUPP: return LINUX_EOPNOTSUPP;
    case ESTALE: return LINUX_ESTALE;
    case EDQUOT: return LINUX_EDQUOT;
    default: return LINUX_EIO;
    }
}

static int linux_to_darwin_oflags(uint32_t lf) {
    int df = O_CLOEXEC;
    uint32_t acc = lf & LINUX_O_ACCMODE;
    if (acc == LINUX_O_WRONLY) {
        df |= O_WRONLY;
    } else if (acc == LINUX_O_RDWR) {
        df |= O_RDWR;
    } else {
        df |= O_RDONLY;
    }
    if (lf & LINUX_O_CREAT) {
        df |= O_CREAT;
    }
    if (lf & LINUX_O_EXCL) {
        df |= O_EXCL;
    }
    if (lf & LINUX_O_TRUNC) {
        df |= O_TRUNC;
    }
    if (lf & LINUX_O_APPEND) {
        df |= O_APPEND;
    }
    if (lf & LINUX_O_NONBLOCK) {
        df |= O_NONBLOCK;
    }
    if (lf & LINUX_O_DIRECTORY) {
        df |= O_DIRECTORY;
    }
    if (lf & LINUX_O_NOFOLLOW) {
        df |= O_NOFOLLOW;
    }
    return df;
}

static void fs_fill_attr(const struct stat *st, uint64_t nodeid, fuse_attr_t *attr) {
    memset(attr, 0, sizeof(*attr));
    attr->ino = nodeid;
    attr->size = (uint64_t)st->st_size;
    attr->blocks = (uint64_t)st->st_blocks;
    attr->atime = (uint64_t)st->st_atimespec.tv_sec;
    attr->atimensec = (uint32_t)st->st_atimespec.tv_nsec;
    attr->mtime = (uint64_t)st->st_mtimespec.tv_sec;
    attr->mtimensec = (uint32_t)st->st_mtimespec.tv_nsec;
    attr->ctime = (uint64_t)st->st_ctimespec.tv_sec;
    attr->ctimensec = (uint32_t)st->st_ctimespec.tv_nsec;
    attr->mode = (uint32_t)st->st_mode;
    attr->nlink = (uint32_t)st->st_nlink;
    attr->uid = (uint32_t)st->st_uid;
    attr->gid = (uint32_t)st->st_gid;
    attr->rdev = (uint32_t)st->st_rdev;
    attr->blksize = st->st_blksize > 0 ? (uint32_t)st->st_blksize : 4096U;
}

static bool fs_name_valid(const char *name) {
    if (!name || name[0] == '\0') {
        return false;
    }
    for (const char *p = name; *p; p++) {
        if (*p == '/') {
            return false;
        }
    }
    return true;
}

static fs_inode_t *fs_find_inode(virtio_fs_t *fs, uint64_t nodeid) {
    for (size_t i = 0; i < fs->inode_cap; i++) {
        if (fs->inodes[i].in_use && fs->inodes[i].nodeid == nodeid) {
            return &fs->inodes[i];
        }
    }
    return NULL;
}

static fs_inode_t *fs_find_inode_by_dev_ino(virtio_fs_t *fs, dev_t dev, ino_t ino) {
    for (size_t i = 0; i < fs->inode_cap; i++) {
        if (fs->inodes[i].in_use && fs->inodes[i].dev == dev && fs->inodes[i].ino == ino) {
            return &fs->inodes[i];
        }
    }
    return NULL;
}

static int fs_open_node_fd(int dirfd, const char *name) {
    int fd = openat(dirfd, name, O_RDONLY | O_SYMLINK | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0 && (errno == EACCES || errno == EPERM)) {
        fd = openat(dirfd, name, O_EVTONLY | O_SYMLINK | O_CLOEXEC);
    }
    return fd;
}

static fs_inode_t *fs_lookup_or_insert(virtio_fs_t *fs, fs_inode_t *parent,
                                       const char *name, struct stat *st_out) {
    if (parent->nodeid == FUSE_ROOT_ID &&
        (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)) {
        if (fstat(parent->fd, st_out) < 0) {
            return NULL;
        }
        parent->nlookup++;
        return parent;
    }

    int fd = fs_open_node_fd(parent->fd, name);
    if (fd < 0) {
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return NULL;
    }

    fs_inode_t *existing = fs_find_inode_by_dev_ino(fs, st.st_dev, st.st_ino);
    if (existing) {
        close(fd);
        existing->nlookup++;
        *st_out = st;
        return existing;
    }

    size_t slot = fs->inode_cap;
    for (size_t i = 0; i < fs->inode_cap; i++) {
        if (!fs->inodes[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot == fs->inode_cap) {
        size_t new_cap = fs->inode_cap ? fs->inode_cap * 2 : 128;
        fs_inode_t *grown = realloc(fs->inodes, new_cap * sizeof(*grown));
        if (!grown) {
            close(fd);
            errno = ENOMEM;
            return NULL;
        }
        memset(grown + fs->inode_cap, 0, (new_cap - fs->inode_cap) * sizeof(*grown));
        fs->inodes = grown;
        slot = fs->inode_cap;
        fs->inode_cap = new_cap;
    }

    fs_inode_t *node = &fs->inodes[slot];
    node->nodeid = fs->next_nodeid++;
    node->generation = fs->generation;
    node->dev = st.st_dev;
    node->ino = st.st_ino;
    node->fd = fd;
    node->nlookup = 1;
    node->in_use = true;
    *st_out = st;
    return node;
}

static void fs_forget_inode(virtio_fs_t *fs, uint64_t nodeid, uint64_t nlookup) {
    if (nodeid == FUSE_ROOT_ID) {
        return;
    }
    fs_inode_t *node = fs_find_inode(fs, nodeid);
    if (!node) {
        return;
    }
    if (nlookup >= node->nlookup) {
        node->nlookup = 0;
        if (node->fd >= 0) {
            close(node->fd);
            node->fd = -1;
        }
        node->in_use = false;
    } else {
        node->nlookup -= nlookup;
    }
}

static void fs_free_dir_entries(fs_handle_t *h) {
    for (size_t i = 0; i < h->entry_count; i++) {
        free(h->entries[i].name);
    }
    free(h->entries);
    h->entries = NULL;
    h->entry_count = 0;
}

static fs_handle_t *fs_find_handle(virtio_fs_t *fs, uint64_t fh) {
    for (size_t i = 0; i < fs->handle_cap; i++) {
        if (fs->handles[i].in_use && fs->handles[i].fh == fh) {
            return &fs->handles[i];
        }
    }
    return NULL;
}

static fs_handle_t *fs_alloc_handle(virtio_fs_t *fs, uint64_t nodeid, int fd, bool is_dir) {
    size_t slot = fs->handle_cap;
    for (size_t i = 0; i < fs->handle_cap; i++) {
        if (!fs->handles[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot == fs->handle_cap) {
        size_t new_cap = fs->handle_cap ? fs->handle_cap * 2 : 64;
        fs_handle_t *grown = realloc(fs->handles, new_cap * sizeof(*grown));
        if (!grown) {
            return NULL;
        }
        memset(grown + fs->handle_cap, 0, (new_cap - fs->handle_cap) * sizeof(*grown));
        fs->handles = grown;
        slot = fs->handle_cap;
        fs->handle_cap = new_cap;
    }
    fs_handle_t *h = &fs->handles[slot];
    memset(h, 0, sizeof(*h));
    h->fh = fs->next_fh++;
    h->nodeid = nodeid;
    h->fd = fd;
    h->is_dir = is_dir;
    h->in_use = true;
    return h;
}

static void fs_release_handle(virtio_fs_t *fs, uint64_t fh) {
    fs_handle_t *h = fs_find_handle(fs, fh);
    if (!h) {
        return;
    }
    fs_free_dir_entries(h);
    if (h->fd >= 0) {
        close(h->fd);
        h->fd = -1;
    }
    h->in_use = false;
}

static int fs_snapshot_dir(fs_handle_t *h) {
    fs_free_dir_entries(h);
    int dup_fd = dup(h->fd);
    if (dup_fd < 0) {
        return -1;
    }
    lseek(dup_fd, 0, SEEK_SET);
    DIR *dir = fdopendir(dup_fd);
    if (!dir) {
        close(dup_fd);
        return -1;
    }

    size_t cap = 32;
    fs_dir_entry_t *entries = malloc(cap * sizeof(*entries));
    if (!entries) {
        closedir(dir);
        errno = ENOMEM;
        return -1;
    }
    size_t count = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (count == cap) {
            size_t new_cap = cap * 2;
            fs_dir_entry_t *grown = realloc(entries, new_cap * sizeof(*grown));
            if (!grown) {
                for (size_t i = 0; i < count; i++) {
                    free(entries[i].name);
                }
                free(entries);
                closedir(dir);
                errno = ENOMEM;
                return -1;
            }
            entries = grown;
            cap = new_cap;
        }
        char *dup_name = strdup(de->d_name);
        if (!dup_name) {
            for (size_t i = 0; i < count; i++) {
                free(entries[i].name);
            }
            free(entries);
            closedir(dir);
            errno = ENOMEM;
            return -1;
        }
        entries[count++] = (fs_dir_entry_t){
            .ino = de->d_ino,
            .type = de->d_type,
            .name = dup_name,
        };
    }
    closedir(dir);
    h->entries = entries;
    h->entry_count = count;
    return 0;
}

static uint32_t fs_reply(const virtio_iov_t *iov, size_t total_iov, uint64_t unique,
                         int32_t error, const void *payload, size_t payload_len) {
    fuse_out_header_t out = {
        .len = (uint32_t)(sizeof(fuse_out_header_t) + payload_len),
        .error = error,
        .unique = unique,
    };
    size_t w = virtio_iov_write(iov, total_iov, 0, &out, sizeof(out));
    if (payload && payload_len > 0) {
        w += virtio_iov_write(iov, total_iov, sizeof(out), payload, payload_len);
    }
    return (uint32_t)w;
}

static void fs_fill_entry_out(const fs_inode_t *node, const struct stat *st, fuse_entry_out_t *eo) {
    memset(eo, 0, sizeof(*eo));
    eo->nodeid = node->nodeid;
    eo->generation = node->generation;
    eo->entry_valid = 1;
    eo->attr_valid = 1;
    fs_fill_attr(st, node->nodeid, &eo->attr);
}

static uint32_t fs_handle_request(virtio_fs_t *fs, const virtio_iov_t *iov, size_t total_iov) {
    fuse_in_header_t in;
    if (virtio_iov_read(iov, total_iov, 0, &in, sizeof(in)) != sizeof(in)) {
        return 0;
    }

    size_t out_total = virtio_iov_total(iov, total_iov, false);
    size_t body_len = out_total > sizeof(in) ? out_total - sizeof(in) : 0;

    switch (in.opcode) {
    case FUSE_INIT: {
        fuse_init_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        uint32_t supported = FUSE_ASYNC_READ | FUSE_ATOMIC_O_TRUNC | FUSE_EXPORT_SUPPORT |
                             FUSE_BIG_WRITES | FUSE_DONT_MASK | FUSE_DO_READDIRPLUS |
                             FUSE_READDIRPLUS_AUTO | FUSE_PARALLEL_DIROPS | FUSE_MAX_PAGES_FLAG;
        fuse_init_out_t resp = {
            .major = FUSE_KERNEL_VERSION,
            .minor = req.minor < FUSE_KERNEL_MINOR_VERSION ? req.minor : FUSE_KERNEL_MINOR_VERSION,
            .max_readahead = req.max_readahead < 131072U ? req.max_readahead : 131072U,
            .flags = req.flags & supported,
            .max_background = 32,
            .congestion_threshold = 24,
            .max_write = FUSE_MAX_WRITE,
            .time_gran = 1,
            .max_pages = FUSE_MAX_PAGES,
        };
        return fs_reply(iov, total_iov, in.unique, 0, &resp, sizeof(resp));
    }

    case FUSE_DESTROY:
    case FUSE_FLUSH:
    case FUSE_INTERRUPT:
        return fs_reply(iov, total_iov, in.unique, 0, NULL, 0);

    case FUSE_FORGET: {
        fuse_forget_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) == sizeof(req)) {
            fs_forget_inode(fs, in.nodeid, req.nlookup);
        }
        return 0;
    }

    case FUSE_BATCH_FORGET: {
        fuse_batch_forget_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) == sizeof(req)) {
            size_t off = sizeof(in) + sizeof(req);
            for (uint32_t i = 0; i < req.count; i++) {
                fuse_forget_one_t one = {0};
                if (virtio_iov_read(iov, total_iov, off, &one, sizeof(one)) != sizeof(one)) {
                    break;
                }
                fs_forget_inode(fs, one.nodeid, one.nlookup);
                off += sizeof(one);
            }
        }
        return 0;
    }

    case FUSE_LOOKUP: {
        fs_inode_t *parent = fs_find_inode(fs, in.nodeid);
        if (!parent) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        char name[NAME_MAX + 1];
        size_t n = body_len < sizeof(name) ? body_len : sizeof(name) - 1;
        virtio_iov_read(iov, total_iov, sizeof(in), name, n);
        name[n] = '\0';
        if (!fs_name_valid(name)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        struct stat st;
        fs_inode_t *child = fs_lookup_or_insert(fs, parent, name, &st);
        if (!child) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fuse_entry_out_t eo;
        fs_fill_entry_out(child, &st, &eo);
        return fs_reply(iov, total_iov, in.unique, 0, &eo, sizeof(eo));
    }

    case FUSE_GETATTR: {
        fuse_getattr_in_t req = {0};
        virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req));
        int fd = -1;
        if (req.getattr_flags & FUSE_GETATTR_FH) {
            fs_handle_t *h = fs_find_handle(fs, req.fh);
            if (h) {
                fd = h->fd;
            }
        }
        if (fd < 0) {
            fs_inode_t *node = fs_find_inode(fs, in.nodeid);
            if (!node) {
                return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
            }
            fd = node->fd;
        }
        struct stat st;
        if (fstat(fd, &st) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fuse_attr_out_t resp = { .attr_valid = 1 };
        fs_fill_attr(&st, in.nodeid, &resp.attr);
        return fs_reply(iov, total_iov, in.unique, 0, &resp, sizeof(resp));
    }

    case FUSE_SETATTR: {
        if (fs->read_only) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EROFS, NULL, 0);
        }
        fuse_setattr_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        fs_inode_t *node = fs_find_inode(fs, in.nodeid);
        if (!node) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        int rw_fd = -1;
        if (req.valid & FATTR_FH) {
            fs_handle_t *h = fs_find_handle(fs, req.fh);
            if (h) {
                rw_fd = h->fd;
            }
        }
        char path[PATH_MAX];
        bool have_path = fcntl(node->fd, F_GETPATH, path) == 0;

        if (req.valid & FATTR_MODE) {
            int rc = fchmod(rw_fd >= 0 ? rw_fd : node->fd, (mode_t)(req.mode & 07777U));
            if (rc < 0 && have_path) {
                rc = chmod(path, (mode_t)(req.mode & 07777U));
            }
            if (rc < 0) {
                return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
            }
        }
        if (req.valid & FATTR_SIZE) {
            int rc = -1;
            if (rw_fd >= 0) {
                rc = ftruncate(rw_fd, (off_t)req.size);
            } else if (have_path) {
                rc = truncate(path, (off_t)req.size);
            } else {
                errno = EBADF;
            }
            if (rc < 0) {
                return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
            }
        }
        if (req.valid & (FATTR_ATIME | FATTR_MTIME | FATTR_ATIME_NOW | FATTR_MTIME_NOW)) {
            struct timespec ts[2];
            if (req.valid & FATTR_ATIME_NOW) {
                ts[0].tv_sec = 0;
                ts[0].tv_nsec = UTIME_NOW;
            } else if (req.valid & FATTR_ATIME) {
                ts[0].tv_sec = (time_t)req.atime;
                ts[0].tv_nsec = (long)req.atimensec;
            } else {
                ts[0].tv_sec = 0;
                ts[0].tv_nsec = UTIME_OMIT;
            }
            if (req.valid & FATTR_MTIME_NOW) {
                ts[1].tv_sec = 0;
                ts[1].tv_nsec = UTIME_NOW;
            } else if (req.valid & FATTR_MTIME) {
                ts[1].tv_sec = (time_t)req.mtime;
                ts[1].tv_nsec = (long)req.mtimensec;
            } else {
                ts[1].tv_sec = 0;
                ts[1].tv_nsec = UTIME_OMIT;
            }
            int rc = futimens(rw_fd >= 0 ? rw_fd : node->fd, ts);
            if (rc < 0 && have_path) {
                rc = utimensat(AT_FDCWD, path, ts, AT_SYMLINK_NOFOLLOW);
            }
            if (rc < 0) {
                return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
            }
        }
        if (req.valid & (FATTR_UID | FATTR_GID)) {
            uid_t uid = (req.valid & FATTR_UID) ? (uid_t)req.uid : (uid_t)-1;
            gid_t gid = (req.valid & FATTR_GID) ? (gid_t)req.gid : (gid_t)-1;
            if (fchown(rw_fd >= 0 ? rw_fd : node->fd, uid, gid) < 0 && errno != EPERM) {
                return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
            }
        }

        struct stat st;
        if (fstat(rw_fd >= 0 ? rw_fd : node->fd, &st) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fuse_attr_out_t resp = { .attr_valid = 1 };
        fs_fill_attr(&st, in.nodeid, &resp.attr);
        return fs_reply(iov, total_iov, in.unique, 0, &resp, sizeof(resp));
    }

    case FUSE_READLINK: {
        fs_inode_t *node = fs_find_inode(fs, in.nodeid);
        if (!node) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        char path[PATH_MAX];
        if (fcntl(node->fd, F_GETPATH, path) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        char target[PATH_MAX];
        ssize_t len = readlink(path, target, sizeof(target));
        if (len < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        return fs_reply(iov, total_iov, in.unique, 0, target, (size_t)len);
    }

    case FUSE_SYMLINK: {
        if (fs->read_only) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EROFS, NULL, 0);
        }
        fs_inode_t *parent = fs_find_inode(fs, in.nodeid);
        if (!parent) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        char buf[PATH_MAX + NAME_MAX + 2];
        size_t n = body_len < sizeof(buf) - 1 ? body_len : sizeof(buf) - 1;
        virtio_iov_read(iov, total_iov, sizeof(in), buf, n);
        buf[n] = '\0';
        size_t name_len = strnlen(buf, n);
        if (name_len + 1 >= n) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        const char *name = buf;
        const char *linkname = buf + name_len + 1;
        if (!fs_name_valid(name)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        if (symlinkat(linkname, parent->fd, name) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        struct stat st;
        fs_inode_t *child = fs_lookup_or_insert(fs, parent, name, &st);
        if (!child) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fuse_entry_out_t eo;
        fs_fill_entry_out(child, &st, &eo);
        return fs_reply(iov, total_iov, in.unique, 0, &eo, sizeof(eo));
    }

    case FUSE_MKNOD: {
        if (fs->read_only) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EROFS, NULL, 0);
        }
        fs_inode_t *parent = fs_find_inode(fs, in.nodeid);
        if (!parent) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        fuse_mknod_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        char name[NAME_MAX + 1];
        size_t rem = body_len > sizeof(req) ? body_len - sizeof(req) : 0;
        size_t n = rem < sizeof(name) ? rem : sizeof(name) - 1;
        virtio_iov_read(iov, total_iov, sizeof(in) + sizeof(req), name, n);
        name[n] = '\0';
        if (!fs_name_valid(name)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        mode_t perm = (mode_t)((req.mode & 07777U) & ~req.umask);
        mode_t ftype = req.mode & S_IFMT;
        if (ftype == 0 || ftype == S_IFREG) {
            int fd = openat(parent->fd, name, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, perm);
            if (fd < 0) {
                return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
            }
            close(fd);
        } else if (ftype == S_IFIFO) {
            char ppath[PATH_MAX];
            if (fcntl(parent->fd, F_GETPATH, ppath) < 0) {
                return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
            }
            char full[PATH_MAX];
            if ((size_t)snprintf(full, sizeof(full), "%s/%s", ppath, name) >= sizeof(full)) {
                return fs_reply(iov, total_iov, in.unique, -LINUX_ENAMETOOLONG, NULL, 0);
            }
            if (mkfifo(full, perm) < 0) {
                return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
            }
        } else {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EPERM, NULL, 0);
        }
        struct stat st;
        fs_inode_t *child = fs_lookup_or_insert(fs, parent, name, &st);
        if (!child) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fuse_entry_out_t eo;
        fs_fill_entry_out(child, &st, &eo);
        return fs_reply(iov, total_iov, in.unique, 0, &eo, sizeof(eo));
    }

    case FUSE_MKDIR: {
        if (fs->read_only) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EROFS, NULL, 0);
        }
        fs_inode_t *parent = fs_find_inode(fs, in.nodeid);
        if (!parent) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        fuse_mkdir_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        char name[NAME_MAX + 1];
        size_t rem = body_len > sizeof(req) ? body_len - sizeof(req) : 0;
        size_t n = rem < sizeof(name) ? rem : sizeof(name) - 1;
        virtio_iov_read(iov, total_iov, sizeof(in) + sizeof(req), name, n);
        name[n] = '\0';
        if (!fs_name_valid(name)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        if (mkdirat(parent->fd, name, (mode_t)((req.mode & 07777U) & ~req.umask)) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        struct stat st;
        fs_inode_t *child = fs_lookup_or_insert(fs, parent, name, &st);
        if (!child) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fuse_entry_out_t eo;
        fs_fill_entry_out(child, &st, &eo);
        return fs_reply(iov, total_iov, in.unique, 0, &eo, sizeof(eo));
    }

    case FUSE_UNLINK:
    case FUSE_RMDIR: {
        if (fs->read_only) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EROFS, NULL, 0);
        }
        fs_inode_t *parent = fs_find_inode(fs, in.nodeid);
        if (!parent) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        char name[NAME_MAX + 1];
        size_t n = body_len < sizeof(name) ? body_len : sizeof(name) - 1;
        virtio_iov_read(iov, total_iov, sizeof(in), name, n);
        name[n] = '\0';
        if (!fs_name_valid(name)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        int flags = (in.opcode == FUSE_RMDIR) ? AT_REMOVEDIR : 0;
        if (unlinkat(parent->fd, name, flags) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        return fs_reply(iov, total_iov, in.unique, 0, NULL, 0);
    }

    case FUSE_RENAME:
    case FUSE_RENAME2: {
        if (fs->read_only) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EROFS, NULL, 0);
        }
        fs_inode_t *olddir = fs_find_inode(fs, in.nodeid);
        if (!olddir) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        uint64_t newdir_id = 0;
        uint32_t rflags = 0;
        size_t hdr_extra = 0;
        if (in.opcode == FUSE_RENAME) {
            fuse_rename_in_t req = {0};
            if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
                return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
            }
            newdir_id = req.newdir;
            hdr_extra = sizeof(req);
        } else {
            fuse_rename2_in_t req = {0};
            if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
                return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
            }
            newdir_id = req.newdir;
            rflags = req.flags;
            hdr_extra = sizeof(req);
        }
        fs_inode_t *newdir = fs_find_inode(fs, newdir_id);
        if (!newdir) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        char buf[2 * (NAME_MAX + 1)];
        size_t rem = body_len > hdr_extra ? body_len - hdr_extra : 0;
        size_t n = rem < sizeof(buf) - 1 ? rem : sizeof(buf) - 1;
        virtio_iov_read(iov, total_iov, sizeof(in) + hdr_extra, buf, n);
        buf[n] = '\0';
        size_t old_len = strnlen(buf, n);
        if (old_len + 1 >= n) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        const char *oldname = buf;
        const char *newname = buf + old_len + 1;
        if (!fs_name_valid(oldname) || !fs_name_valid(newname)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        int rc;
        if (rflags == 0) {
            rc = renameat(olddir->fd, oldname, newdir->fd, newname);
        } else {
            unsigned int dflags = 0;
            if (rflags & ~(LINUX_RENAME_NOREPLACE | LINUX_RENAME_EXCHANGE)) {
                return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
            }
            if (rflags & LINUX_RENAME_NOREPLACE) {
                dflags |= RENAME_EXCL;
            }
            if (rflags & LINUX_RENAME_EXCHANGE) {
                dflags |= RENAME_SWAP;
            }
            rc = renameatx_np(olddir->fd, oldname, newdir->fd, newname, dflags);
        }
        if (rc < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        return fs_reply(iov, total_iov, in.unique, 0, NULL, 0);
    }

    case FUSE_LINK: {
        if (fs->read_only) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EROFS, NULL, 0);
        }
        fs_inode_t *newdir = fs_find_inode(fs, in.nodeid);
        if (!newdir) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        fuse_link_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        fs_inode_t *oldnode = fs_find_inode(fs, req.oldnodeid);
        if (!oldnode) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        char newname[NAME_MAX + 1];
        size_t rem = body_len > sizeof(req) ? body_len - sizeof(req) : 0;
        size_t n = rem < sizeof(newname) ? rem : sizeof(newname) - 1;
        virtio_iov_read(iov, total_iov, sizeof(in) + sizeof(req), newname, n);
        newname[n] = '\0';
        if (!fs_name_valid(newname)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        char oldpath[PATH_MAX];
        if (fcntl(oldnode->fd, F_GETPATH, oldpath) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        if (linkat(AT_FDCWD, oldpath, newdir->fd, newname, 0) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        struct stat st;
        fs_inode_t *child = fs_lookup_or_insert(fs, newdir, newname, &st);
        if (!child) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fuse_entry_out_t eo;
        fs_fill_entry_out(child, &st, &eo);
        return fs_reply(iov, total_iov, in.unique, 0, &eo, sizeof(eo));
    }

    case FUSE_OPEN: {
        fuse_open_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        uint32_t acc = req.flags & LINUX_O_ACCMODE;
        if (fs->read_only &&
            (acc == LINUX_O_WRONLY || acc == LINUX_O_RDWR ||
             (req.flags & (LINUX_O_CREAT | LINUX_O_TRUNC | LINUX_O_APPEND)))) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EROFS, NULL, 0);
        }
        fs_inode_t *node = fs_find_inode(fs, in.nodeid);
        if (!node) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        char path[PATH_MAX];
        if (fcntl(node->fd, F_GETPATH, path) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        int dflags = linux_to_darwin_oflags(req.flags) & ~(O_CREAT | O_EXCL);
        int fd = open(path, dflags);
        if (fd < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fs_handle_t *h = fs_alloc_handle(fs, node->nodeid, fd, false);
        if (!h) {
            close(fd);
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOMEM, NULL, 0);
        }
        fuse_open_out_t resp = { .fh = h->fh };
        return fs_reply(iov, total_iov, in.unique, 0, &resp, sizeof(resp));
    }

    case FUSE_CREATE: {
        if (fs->read_only) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EROFS, NULL, 0);
        }
        fs_inode_t *parent = fs_find_inode(fs, in.nodeid);
        if (!parent) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        fuse_create_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        char name[NAME_MAX + 1];
        size_t rem = body_len > sizeof(req) ? body_len - sizeof(req) : 0;
        size_t n = rem < sizeof(name) ? rem : sizeof(name) - 1;
        virtio_iov_read(iov, total_iov, sizeof(in) + sizeof(req), name, n);
        name[n] = '\0';
        if (!fs_name_valid(name)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        int dflags = linux_to_darwin_oflags(req.flags) | O_CREAT;
        mode_t perm = (mode_t)((req.mode & 07777U) & ~req.umask);
        int fd = openat(parent->fd, name, dflags, perm);
        if (fd < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        struct stat st;
        fs_inode_t *child = fs_lookup_or_insert(fs, parent, name, &st);
        if (!child) {
            int saved = errno;
            close(fd);
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(saved), NULL, 0);
        }
        fs_handle_t *h = fs_alloc_handle(fs, child->nodeid, fd, false);
        if (!h) {
            close(fd);
            fs_forget_inode(fs, child->nodeid, 1);
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOMEM, NULL, 0);
        }
        struct {
            fuse_entry_out_t entry;
            fuse_open_out_t open;
        } resp;
        fs_fill_entry_out(child, &st, &resp.entry);
        resp.open = (fuse_open_out_t){ .fh = h->fh };
        return fs_reply(iov, total_iov, in.unique, 0, &resp, sizeof(resp));
    }

    case FUSE_READ: {
        fuse_read_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        fs_handle_t *h = fs_find_handle(fs, req.fh);
        if (!h || h->is_dir) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EBADF, NULL, 0);
        }
        size_t in_cap = virtio_iov_total(iov, total_iov, true);
        size_t max_bytes = in_cap > sizeof(fuse_out_header_t) ? in_cap - sizeof(fuse_out_header_t) : 0;
        size_t to_read = req.size < max_bytes ? req.size : max_bytes;
        uint8_t *buf = to_read > 0 ? malloc(to_read) : NULL;
        if (to_read > 0 && !buf) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOMEM, NULL, 0);
        }
        ssize_t got = 0;
        if (to_read > 0) {
            do {
                got = pread(h->fd, buf, to_read, (off_t)req.offset);
            } while (got < 0 && errno == EINTR);
        }
        if (got < 0) {
            int saved = errno;
            free(buf);
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(saved), NULL, 0);
        }
        uint32_t written = fs_reply(iov, total_iov, in.unique, 0, buf, (size_t)got);
        free(buf);
        return written;
    }

    case FUSE_WRITE: {
        if (fs->read_only) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EROFS, NULL, 0);
        }
        fuse_write_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        fs_handle_t *h = fs_find_handle(fs, req.fh);
        if (!h || h->is_dir) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EBADF, NULL, 0);
        }
        size_t avail = body_len > sizeof(req) ? body_len - sizeof(req) : 0;
        size_t to_write = req.size < avail ? req.size : avail;
        uint8_t *buf = to_write > 0 ? malloc(to_write) : NULL;
        if (to_write > 0 && !buf) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOMEM, NULL, 0);
        }
        if (to_write > 0) {
            virtio_iov_read(iov, total_iov, sizeof(in) + sizeof(req), buf, to_write);
        }
        ssize_t n = 0;
        if (to_write > 0) {
            do {
                n = pwrite(h->fd, buf, to_write, (off_t)req.offset);
            } while (n < 0 && errno == EINTR);
        }
        free(buf);
        if (n < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fuse_write_out_t resp = { .size = (uint32_t)n };
        return fs_reply(iov, total_iov, in.unique, 0, &resp, sizeof(resp));
    }

    case FUSE_STATFS: {
        fs_inode_t *node = fs_find_inode(fs, in.nodeid);
        if (!node) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        struct statvfs stv;
        if (fstatvfs(node->fd, &stv) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fuse_kstatfs_t ks = {
            .blocks = (uint64_t)stv.f_blocks,
            .bfree = (uint64_t)stv.f_bfree,
            .bavail = (uint64_t)stv.f_bavail,
            .files = (uint64_t)stv.f_files,
            .ffree = (uint64_t)stv.f_ffree,
            .bsize = (uint32_t)stv.f_bsize,
            .namelen = (uint32_t)stv.f_namemax,
            .frsize = (uint32_t)stv.f_frsize,
        };
        return fs_reply(iov, total_iov, in.unique, 0, &ks, sizeof(ks));
    }

    case FUSE_RELEASE:
    case FUSE_RELEASEDIR: {
        fuse_release_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) == sizeof(req)) {
            fs_release_handle(fs, req.fh);
        }
        return fs_reply(iov, total_iov, in.unique, 0, NULL, 0);
    }

    case FUSE_FSYNC:
    case FUSE_FSYNCDIR: {
        fuse_fsync_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        fs_handle_t *h = fs_find_handle(fs, req.fh);
        int fd = h ? h->fd : -1;
        if (fd < 0) {
            fs_inode_t *node = fs_find_inode(fs, in.nodeid);
            if (!node) {
                return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
            }
            fd = node->fd;
        }
        if (fsync(fd) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        return fs_reply(iov, total_iov, in.unique, 0, NULL, 0);
    }

    case FUSE_OPENDIR: {
        fs_inode_t *node = fs_find_inode(fs, in.nodeid);
        if (!node) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        char path[PATH_MAX];
        if (fcntl(node->fd, F_GETPATH, path) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fs_handle_t *h = fs_alloc_handle(fs, node->nodeid, fd, true);
        if (!h) {
            close(fd);
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOMEM, NULL, 0);
        }
        if (fs_snapshot_dir(h) < 0) {
            int saved = errno;
            fs_release_handle(fs, h->fh);
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(saved), NULL, 0);
        }
        fuse_open_out_t resp = { .fh = h->fh };
        return fs_reply(iov, total_iov, in.unique, 0, &resp, sizeof(resp));
    }

    case FUSE_READDIR:
    case FUSE_READDIRPLUS: {
        fuse_read_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        fs_handle_t *h = fs_find_handle(fs, req.fh);
        if (!h || !h->is_dir) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EBADF, NULL, 0);
        }
        fs_inode_t *dir_node = fs_find_inode(fs, h->nodeid);
        if (!dir_node) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOENT, NULL, 0);
        }
        if (req.offset == 0 && fs_snapshot_dir(h) < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }

        size_t in_cap = virtio_iov_total(iov, total_iov, true);
        size_t max_bytes = in_cap > sizeof(fuse_out_header_t) ? in_cap - sizeof(fuse_out_header_t) : 0;
        size_t limit = req.size < max_bytes ? req.size : max_bytes;
        uint8_t *buf = limit > 0 ? calloc(1, limit) : NULL;
        if (limit > 0 && !buf) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_ENOMEM, NULL, 0);
        }

        bool plus = (in.opcode == FUSE_READDIRPLUS);
        size_t pos = 0;
        for (size_t idx = (size_t)req.offset; buf && idx < h->entry_count; idx++) {
            const fs_dir_entry_t *e = &h->entries[idx];
            size_t namelen = strlen(e->name);
            size_t hdr_sz = plus ? sizeof(fuse_direntplus_t) : sizeof(fuse_dirent_t);
            size_t rec_sz = (hdr_sz + namelen + 7U) & ~(size_t)7U;
            if (pos + rec_sz > limit) {
                break;
            }
            if (plus) {
                fuse_direntplus_t dp;
                memset(&dp, 0, sizeof(dp));
                dp.dirent.ino = e->ino;
                dp.dirent.off = (uint64_t)(idx + 1);
                dp.dirent.namelen = (uint32_t)namelen;
                dp.dirent.type = e->type;
                if (strcmp(e->name, ".") != 0 && strcmp(e->name, "..") != 0) {
                    struct stat st;
                    fs_inode_t *child = fs_lookup_or_insert(fs, dir_node, e->name, &st);
                    if (child) {
                        fs_fill_entry_out(child, &st, &dp.entry_out);
                        dp.dirent.ino = child->nodeid;
                        dp.dirent.type = (uint32_t)((st.st_mode & S_IFMT) >> 12);
                    }
                }
                memcpy(buf + pos, &dp, sizeof(dp));
                memcpy(buf + pos + sizeof(dp), e->name, namelen);
            } else {
                fuse_dirent_t d = {
                    .ino = e->ino,
                    .off = (uint64_t)(idx + 1),
                    .namelen = (uint32_t)namelen,
                    .type = e->type,
                };
                memcpy(buf + pos, &d, sizeof(d));
                memcpy(buf + pos + sizeof(d), e->name, namelen);
            }
            pos += rec_sz;
        }

        uint32_t written = fs_reply(iov, total_iov, in.unique, 0, buf, pos);
        free(buf);
        return written;
    }

    case FUSE_LSEEK: {
        fuse_lseek_in_t req = {0};
        if (virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req)) < sizeof(req)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        fs_handle_t *h = fs_find_handle(fs, req.fh);
        if (!h || h->is_dir) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EBADF, NULL, 0);
        }
        int whence;
        switch (req.whence) {
        case 0: whence = SEEK_SET; break;
        case 1: whence = SEEK_CUR; break;
        case 2: whence = SEEK_END; break;
        case 3: whence = SEEK_DATA; break;
        case 4: whence = SEEK_HOLE; break;
        default:
            return fs_reply(iov, total_iov, in.unique, -LINUX_EINVAL, NULL, 0);
        }
        off_t off = lseek(h->fd, (off_t)req.offset, whence);
        if (off < 0) {
            return fs_reply(iov, total_iov, in.unique, -darwin_to_linux_errno(errno), NULL, 0);
        }
        fuse_lseek_out_t resp = { .offset = (uint64_t)off };
        return fs_reply(iov, total_iov, in.unique, 0, &resp, sizeof(resp));
    }

    case FUSE_ACCESS: {
        fuse_access_in_t req = {0};
        virtio_iov_read(iov, total_iov, sizeof(in), &req, sizeof(req));
        if (fs->read_only && (req.mask & W_OK)) {
            return fs_reply(iov, total_iov, in.unique, -LINUX_EROFS, NULL, 0);
        }
        return fs_reply(iov, total_iov, in.unique, 0, NULL, 0);
    }

    case FUSE_SETXATTR:
    case FUSE_REMOVEXATTR:
    case FUSE_FALLOCATE:
        return fs_reply(iov, total_iov, in.unique, fs->read_only ? -LINUX_EROFS : -LINUX_ENOSYS, NULL, 0);

    case FUSE_GETXATTR:
    case FUSE_LISTXATTR:
    default:
        return fs_reply(iov, total_iov, in.unique, -LINUX_ENOSYS, NULL, 0);
    }
}

static void fs_drain_queue(virtio_fs_t *fs, uint32_t qidx) {
    virtio_dev_t *dev = fs->dev;
    virtqueue_t *vq = &dev->queues[qidx];
    if (!atomic_load_explicit(&vq->ready, memory_order_acquire) ||
        !(atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_DRIVER_OK)) {
        return;
    }
    uint16_t old_used = vq->used_idx;
    bool pushed = false;

    for (;;) {
        virtio_iov_t iov[VIRTIO_FS_MAX_IOV];
        uint16_t head = 0;
        size_t out_cnt = 0;
        size_t in_cnt = 0;
        if (virtqueue_pop(dev, vq, iov, VIRTIO_FS_MAX_IOV, &head, &out_cnt, &in_cnt) <= 0) {
            break;
        }
        uint32_t written = fs_handle_request(fs, iov, out_cnt + in_cnt);
        virtqueue_push(dev, vq, head, written);
        pushed = true;
    }

    if (pushed && virtqueue_should_notify(dev, vq, old_used)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
}

static void *virtio_fs_worker(void *ctx) {
    virtio_fs_t *fs = ctx;
    pthread_mutex_lock(&fs->lock);
    for (;;) {
        while (!fs->pending && !fs->stop) {
            pthread_cond_wait(&fs->work_cond, &fs->lock);
        }
        if (!fs->pending && fs->stop) {
            break;
        }
        fs->pending = false;
        fs->busy = true;
        pthread_mutex_unlock(&fs->lock);

        fs_drain_queue(fs, VIRTIO_FS_VQ_HIPRIO);
        fs_drain_queue(fs, VIRTIO_FS_VQ_REQUEST);

        pthread_mutex_lock(&fs->lock);
        fs->busy = false;
        if (!fs->pending) {
            pthread_cond_broadcast(&fs->idle_cond);
        }
    }
    pthread_mutex_unlock(&fs->lock);
    return NULL;
}

static uint64_t virtio_fs_read_config(virtio_dev_t *dev, uint32_t offset, uint32_t size) {
    virtio_fs_t *fs = dev->priv;
    return virtio_config_read_buf(&fs->config, sizeof(fs->config), offset, size);
}

static void virtio_fs_queue_notify(virtio_dev_t *dev, uint32_t queue_idx) {
    if (queue_idx >= dev->num_queues) {
        return;
    }
    virtio_fs_t *fs = dev->priv;
    pthread_mutex_lock(&fs->lock);
    if (!fs->stop) {
        fs->pending = true;
        pthread_cond_signal(&fs->work_cond);
    }
    pthread_mutex_unlock(&fs->lock);
}

static void virtio_fs_reset_tables(virtio_fs_t *fs) {
    for (size_t i = 0; i < fs->handle_cap; i++) {
        if (fs->handles[i].in_use) {
            fs_free_dir_entries(&fs->handles[i]);
            if (fs->handles[i].fd >= 0) {
                close(fs->handles[i].fd);
                fs->handles[i].fd = -1;
            }
            fs->handles[i].in_use = false;
        }
    }
    for (size_t i = 0; i < fs->inode_cap; i++) {
        if (fs->inodes[i].in_use && fs->inodes[i].nodeid != FUSE_ROOT_ID) {
            if (fs->inodes[i].fd >= 0) {
                close(fs->inodes[i].fd);
                fs->inodes[i].fd = -1;
            }
            fs->inodes[i].in_use = false;
        }
    }
    fs->generation++;
}

static void virtio_fs_reset(virtio_dev_t *dev) {
    virtio_fs_t *fs = dev->priv;
    pthread_mutex_lock(&fs->lock);
    while (fs->pending || fs->busy) {
        pthread_cond_wait(&fs->idle_cond, &fs->lock);
    }
    pthread_mutex_unlock(&fs->lock);
    virtio_fs_reset_tables(fs);
}

static void virtio_fs_destroy(virtio_dev_t *dev) {
    virtio_fs_t *fs = dev->priv;
    if (!fs) {
        return;
    }
    pthread_mutex_lock(&fs->lock);
    fs->stop = true;
    pthread_cond_signal(&fs->work_cond);
    pthread_mutex_unlock(&fs->lock);
    if (fs->worker_started) {
        pthread_join(fs->worker, NULL);
        fs->worker_started = false;
    }
    pthread_cond_destroy(&fs->idle_cond);
    pthread_cond_destroy(&fs->work_cond);
    pthread_mutex_destroy(&fs->lock);

    virtio_fs_reset_tables(fs);
    for (size_t i = 0; i < fs->inode_cap; i++) {
        if (fs->inodes[i].in_use && fs->inodes[i].fd >= 0) {
            close(fs->inodes[i].fd);
        }
    }
    free(fs->inodes);
    free(fs->handles);
    free(fs);
    dev->priv = NULL;
}

static const virtio_dev_ops_t virtio_fs_ops = {
    .read_config = virtio_fs_read_config,
    .queue_notify = virtio_fs_queue_notify,
    .reset = virtio_fs_reset,
    .destroy = virtio_fs_destroy,
};

virtio_dev_t *virtio_fs_create(vmm_machine_t *vm, uint32_t slot,
                               const char *tag, const char *host_root, bool read_only) {
    if (!vm || !tag || !host_root || tag[0] == '\0') {
        return NULL;
    }
    int root_fd = open(host_root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (root_fd < 0) {
        return NULL;
    }
    struct stat st;
    if (fstat(root_fd, &st) < 0) {
        close(root_fd);
        return NULL;
    }

    virtio_fs_t *fs = calloc(1, sizeof(*fs));
    if (!fs) {
        close(root_fd);
        return NULL;
    }
    fs->read_only = read_only;
    size_t tag_len = strnlen(tag, VIRTIO_FS_TAG_LEN);
    memcpy(fs->config.tag, tag, tag_len);
    fs->config.num_request_queues = 1;
    fs->next_nodeid = FUSE_ROOT_ID + 1;
    fs->next_fh = 1;
    fs->generation = 1;

    fs->inodes = calloc(128, sizeof(*fs->inodes));
    fs->handles = calloc(64, sizeof(*fs->handles));
    if (!fs->inodes || !fs->handles) {
        free(fs->inodes);
        free(fs->handles);
        close(root_fd);
        free(fs);
        return NULL;
    }
    fs->inode_cap = 128;
    fs->handle_cap = 64;
    fs->inodes[0] = (fs_inode_t){
        .nodeid = FUSE_ROOT_ID,
        .generation = 0,
        .dev = st.st_dev,
        .ino = st.st_ino,
        .fd = root_fd,
        .nlookup = 1,
        .in_use = true,
    };

    pthread_mutex_init(&fs->lock, NULL);
    pthread_cond_init(&fs->work_cond, NULL);
    pthread_cond_init(&fs->idle_cond, NULL);

    const uint32_t qsizes[2] = { VIRTIO_FS_HIPRIO_SIZE, VIRTIO_FS_REQ_SIZE };
    virtio_dev_t *dev = virtio_dev_alloc(vm, slot, VIRTIO_ID_FS, 2, qsizes, &virtio_fs_ops, fs);
    if (!dev) {
        pthread_cond_destroy(&fs->idle_cond);
        pthread_cond_destroy(&fs->work_cond);
        pthread_mutex_destroy(&fs->lock);
        free(fs->inodes);
        free(fs->handles);
        close(root_fd);
        free(fs);
        return NULL;
    }
    fs->dev = dev;
    if (pthread_create(&fs->worker, NULL, virtio_fs_worker, fs) != 0) {
        virtio_dev_destroy(dev);
        return NULL;
    }
    fs->worker_started = true;
    return dev;
}
