#include "swiptermd.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/openat2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

typedef struct active_mount {
    char *guest_path;
    char dev_name[32];
} active_mount_t;

static active_mount_t *g_mounts;
static size_t g_mount_count;
static uint8_t g_resolver[4096];
static size_t g_resolver_len;

static void sync_root_fs(void) {
    int fd = open(GUEST_ROOT, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd >= 0) {
        (void)syncfs(fd);
        close(fd);
    }
}

static bool write_sysfs_str(const char *path, const char *val) {
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    ssize_t len = (ssize_t)strlen(val);
    ssize_t n = write(fd, val, (size_t)len);
    close(fd);
    return n == len;
}

static void bind_virtio_fs_slots(char bound_dev[32]) {
    bound_dev[0] = '\0';
    for (unsigned slot = 5; slot < 13; slot++) {
        char dev[32];
        char drv[128];
        snprintf(dev, sizeof(dev), "%x.virtio_mmio", 0xa000000U + slot * 0x200U);
        snprintf(drv, sizeof(drv), "/sys/bus/platform/devices/%s/driver", dev);
        struct stat st;
        if (lstat(drv, &st) == 0)
            continue;
        if (write_sysfs_str("/sys/bus/platform/drivers/virtio-mmio/bind", dev)) {
            if (bound_dev[0] == '\0')
                strcpy(bound_dev, dev);
        }
    }
}

int fs_open_in_root(const char *rel, uint64_t flags, uint64_t mode) {
    struct open_how how;
    memset(&how, 0, sizeof(how));
    how.flags = flags | O_CLOEXEC;
    how.mode = mode;
    how.resolve = RESOLVE_IN_ROOT;
    long fd = syscall(SYS_openat2, g_daemon.root_fd, rel, &how, sizeof(how));
    if (fd < 0)
        return -errno;
    return (int)fd;
}

static int normalize_guest_path(const char *in, char *out, size_t cap) {
    if (!in || in[0] == '\0')
        return -EINVAL;
    char tmp[PATH_MAX];
    if (strlen(in) >= sizeof(tmp))
        return -ENAMETOOLONG;

    const char *parts[256];
    size_t lens[256];
    size_t depth = 0;

    const char *p = in;
    while (*p) {
        while (*p == '/')
            p++;
        if (*p == '\0')
            break;
        const char *start = p;
        while (*p && *p != '/')
            p++;
        size_t len = (size_t)(p - start);
        if (len == 1 && start[0] == '.')
            continue;
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (depth > 0)
                depth--;
            continue;
        }
        if (depth >= 256)
            return -ENAMETOOLONG;
        parts[depth] = start;
        lens[depth] = len;
        depth++;
    }

    size_t pos = 0;
    if (depth == 0) {
        if (cap < 2)
            return -ENAMETOOLONG;
        out[0] = '/';
        out[1] = '\0';
        return 0;
    }
    for (size_t i = 0; i < depth; i++) {
        if (pos + 1 + lens[i] + 1 > cap)
            return -ENAMETOOLONG;
        out[pos++] = '/';
        memcpy(out + pos, parts[i], lens[i]);
        pos += lens[i];
    }
    out[pos] = '\0';
    return 0;
}

static const char *to_rel_path(const char *guest_path) {
    while (*guest_path == '/')
        guest_path++;
    return *guest_path ? guest_path : ".";
}

static int split_parent_name(const char *guest_path, char *parent_out, size_t pcap,
                             char *name_out, size_t ncap) {
    char norm[PATH_MAX];
    int err = normalize_guest_path(guest_path, norm, sizeof(norm));
    if (err < 0)
        return err;
    if (strcmp(norm, "/") == 0)
        return -EISDIR;
    char *slash = strrchr(norm, '/');
    if (!slash || slash[1] == '\0')
        return -EISDIR;
    if (strlen(slash + 1) >= ncap)
        return -ENAMETOOLONG;
    strcpy(name_out, slash + 1);
    if (slash == norm) {
        if (pcap < 2)
            return -ENAMETOOLONG;
        strcpy(parent_out, "/");
    } else {
        *slash = '\0';
        if (strlen(norm) >= pcap)
            return -ENAMETOOLONG;
        strcpy(parent_out, norm);
    }
    return 0;
}

static bool path_contains(const char *ancestor, const char *path) {
    if (strcmp(ancestor, "/") == 0)
        return true;
    size_t len = strlen(ancestor);
    return strncmp(ancestor, path, len) == 0 && (path[len] == '/' || path[len] == '\0');
}

int fs_mkdir_p_jailed(const char *guest_path, uint32_t mode) {
    char norm[PATH_MAX];
    int err = normalize_guest_path(guest_path, norm, sizeof(norm));
    if (err < 0)
        return err;
    if (strcmp(norm, "/") == 0)
        return 0;

    int cur_fd = fs_open_in_root(".", O_PATH | O_DIRECTORY, 0);
    if (cur_fd < 0)
        return cur_fd;

    char *p = norm + 1;
    while (*p) {
        char *slash = strchr(p, '/');
        if (slash)
            *slash = '\0';

        if (mkdirat(cur_fd, p, mode & 07777) < 0) {
            if (errno != EEXIST) {
                err = -errno;
                close(cur_fd);
                return err;
            }
            struct stat st;
            if (fstatat(cur_fd, p, &st, 0) < 0) {
                err = -errno;
                close(cur_fd);
                return err;
            }
            if (!S_ISDIR(st.st_mode)) {
                close(cur_fd);
                return -ENOTDIR;
            }
        }

        int next_fd = openat(cur_fd, p, O_PATH | O_DIRECTORY | O_CLOEXEC);
        close(cur_fd);
        if (next_fd < 0)
            return -errno;
        cur_fd = next_fd;

        if (!slash)
            break;
        p = slash + 1;
    }

    close(cur_fd);
    return 0;
}

int fs_open_parent(const char *guest_path, char *name_out, size_t name_cap) {
    char parent[PATH_MAX];
    int err = split_parent_name(guest_path, parent, sizeof(parent), name_out, name_cap);
    if (err < 0)
        return err;
    return fs_open_in_root(to_rel_path(parent), O_PATH | O_DIRECTORY, 0);
}

int fs_write_file_jailed(const char *guest_path, const uint8_t *data, size_t len, uint32_t mode) {
    char parent[PATH_MAX];
    char name[NAME_MAX + 1];
    int err = split_parent_name(guest_path, parent, sizeof(parent), name, sizeof(name));
    if (err < 0)
        return err;

    if (strcmp(parent, "/") != 0) {
        err = fs_mkdir_p_jailed(parent, 0755);
        if (err < 0)
            return err;
    }

    int pfd = fs_open_in_root(to_rel_path(parent), O_PATH | O_DIRECTORY, 0);
    if (pfd < 0)
        return pfd;

    uint32_t rnd = 0;
    (void)getrandom(&rnd, sizeof(rnd), 0);
    char tmp_name[NAME_MAX + 1];
    if (snprintf(tmp_name, sizeof(tmp_name), ".%s.%08x", name, rnd) >= (int)sizeof(tmp_name)) {
        close(pfd);
        return -ENAMETOOLONG;
    }

    int fd = openat(pfd, tmp_name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode & 07777);
    if (fd < 0) {
        err = -errno;
        close(pfd);
        return err;
    }

    err = write_all_blocking(fd, data, len);
    if (err == 0 && fchmod(fd, mode & 07777) < 0)
        err = -errno;
    close(fd);

    if (err == 0 && renameat(pfd, tmp_name, pfd, name) < 0)
        err = -errno;
    if (err < 0)
        unlinkat(pfd, tmp_name, 0);

    close(pfd);
    return err;
}

void fs_handle_stat(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    bool follow = swiptermd_r_u8(&r) != 0;
    char path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, path, sizeof(path))) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_STAT, req_id, -EINVAL);
        return;
    }

    uint64_t flags = O_PATH | (follow ? 0 : O_NOFOLLOW);
    int fd = fs_open_in_root(to_rel_path(path), flags, 0);
    if (fd < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_STAT, req_id, fd);
        return;
    }

    struct stat st;
    if (fstatat(fd, "", &st, AT_EMPTY_PATH) < 0) {
        int err = -errno;
        close(fd);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_STAT, req_id, err);
        return;
    }
    close(fd);

    swiptermd_stat_wire_t wire = {
        .mode = st.st_mode,
        .uid = st.st_uid,
        .gid = st.st_gid,
        .nlink = (uint32_t)st.st_nlink,
        .size = (uint64_t)st.st_size,
        .inode = (uint64_t)st.st_ino,
        .rdev = (uint64_t)st.st_rdev,
        .atime_sec = st.st_atim.tv_sec,
        .atime_nsec = st.st_atim.tv_nsec,
        .mtime_sec = st.st_mtim.tv_sec,
        .mtime_nsec = st.st_mtim.tv_nsec,
        .ctime_sec = st.st_ctim.tv_sec,
        .ctime_nsec = st.st_ctim.tv_nsec,
    };

    uint8_t out[SWIPTERMD_ERR_REPLY_SIZE + SWIPTERMD_STAT_WIRE_SIZE];
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, sizeof(out));
    swiptermd_w_i32(&w, 0);
    swiptermd_stat_encode(&w, &wire);
    ctrl_send_frame((uint16_t)(SWIPTERMD_OP_FS_STAT | SWIPTERMD_OP_REPLY_FLAG),
                    req_id, out, (uint32_t)w.len);
}

void fs_handle_read(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    char path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, path, sizeof(path))) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_READ, req_id, -EINVAL);
        return;
    }
    uint64_t offset = swiptermd_r_u64(&r);
    uint32_t max_len = swiptermd_r_u32(&r);
    if (r.underflow) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_READ, req_id, -EINVAL);
        return;
    }

    int fd = fs_open_in_root(to_rel_path(path), O_RDONLY, 0);
    if (fd < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_READ, req_id, fd);
        return;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        int err = -errno;
        close(fd);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_READ, req_id, err);
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        close(fd);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_READ, req_id, -EISDIR);
        return;
    }

    uint32_t limit = SWIPTERMD_MAX_BODY_LEN - 32;
    if (max_len == 0 || max_len > limit)
        max_len = limit;

    uint8_t *out = malloc(9 + (size_t)max_len);
    if (!out) {
        close(fd);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_READ, req_id, -ENOMEM);
        return;
    }

    size_t got = 0;
    bool eof = false;
    if (offset > 0 && lseek(fd, (off_t)offset, SEEK_SET) < 0) {
        int err = -errno;
        free(out);
        close(fd);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_READ, req_id, err);
        return;
    }

    while (got < max_len) {
        ssize_t n = read(fd, out + 9 + got, max_len - got);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            int err = -errno;
            free(out);
            close(fd);
            ctrl_send_err_reply(SWIPTERMD_OP_FS_READ, req_id, err);
            return;
        }
        if (n == 0) {
            eof = true;
            break;
        }
        got += (size_t)n;
    }
    close(fd);

    swiptermd_put_be32(out + 0, 0);
    out[4] = eof ? 1 : 0;
    swiptermd_put_be32(out + 5, (uint32_t)got);
    ctrl_send_frame((uint16_t)(SWIPTERMD_OP_FS_READ | SWIPTERMD_OP_REPLY_FLAG),
                    req_id, out, (uint32_t)(9 + got));
    free(out);
}

void fs_handle_write(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    uint32_t mode = swiptermd_r_u32(&r);
    uint32_t flags = swiptermd_r_u32(&r);
    char path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, path, sizeof(path))) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_WRITE, req_id, -EINVAL);
        return;
    }
    swiptermd_slice_t data = swiptermd_r_bytes(&r);
    if (r.underflow) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_WRITE, req_id, -EINVAL);
        return;
    }

    int err;
    if (flags & SWIPTERMD_FS_WRITE_FLAG_APPEND) {
        int fd = fs_open_in_root(to_rel_path(path), O_WRONLY | O_CREAT | O_APPEND, mode & 07777);
        if (fd < 0) {
            err = fd;
        } else {
            err = write_all_blocking(fd, data.ptr, data.len);
            close(fd);
        }
    } else {
        err = fs_write_file_jailed(path, data.ptr, data.len, mode);
    }
    if (err == 0)
        sync_root_fs();
    ctrl_send_err_reply(SWIPTERMD_OP_FS_WRITE, req_id, err);
}

void fs_handle_list(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    char path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, path, sizeof(path))) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_LIST, req_id, -EINVAL);
        return;
    }

    int dfd = fs_open_in_root(to_rel_path(path), O_RDONLY | O_DIRECTORY, 0);
    if (dfd < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_LIST, req_id, dfd);
        return;
    }
    DIR *dir = fdopendir(dfd);
    if (!dir) {
        int err = -errno;
        close(dfd);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_LIST, req_id, err);
        return;
    }

    size_t cap = 4096;
    uint8_t *out = malloc(cap);
    if (!out) {
        closedir(dir);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_LIST, req_id, -ENOMEM);
        return;
    }
    size_t pos = 8;
    uint32_t count = 0;

    struct dirent *de;
    errno = 0;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        struct stat st;
        memset(&st, 0, sizeof(st));
        (void)fstatat(dfd, de->d_name, &st, AT_SYMLINK_NOFOLLOW);

        uint32_t nlen = (uint32_t)strlen(de->d_name);
        size_t need = 8 + (size_t)nlen;
        if (pos + need > cap) {
            while (pos + need > cap)
                cap *= 2;
            uint8_t *nb = realloc(out, cap);
            if (!nb) {
                free(out);
                closedir(dir);
                ctrl_send_err_reply(SWIPTERMD_OP_FS_LIST, req_id, -ENOMEM);
                return;
            }
            out = nb;
        }
        swiptermd_buf_w_t ew = swiptermd_buf_w_init(out + pos, cap - pos);
        swiptermd_fs_list_entry_encode(&ew, st.st_mode, de->d_name);
        pos += ew.len;
        count++;
        errno = 0;
    }
    int rerr = errno;
    closedir(dir);
    if (rerr != 0) {
        free(out);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_LIST, req_id, -rerr);
        return;
    }

    swiptermd_put_be32(out + 0, 0);
    swiptermd_put_be32(out + 4, count);
    ctrl_send_frame((uint16_t)(SWIPTERMD_OP_FS_LIST | SWIPTERMD_OP_REPLY_FLAG),
                    req_id, out, (uint32_t)pos);
    free(out);
}

void fs_handle_mkdir(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    uint32_t mode = swiptermd_r_u32(&r);
    bool intermediates = swiptermd_r_u8(&r) != 0;
    char path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, path, sizeof(path))) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_MKDIR, req_id, -EINVAL);
        return;
    }

    int err;
    if (intermediates) {
        err = fs_mkdir_p_jailed(path, mode);
    } else {
        char parent[PATH_MAX];
        char name[NAME_MAX + 1];
        err = split_parent_name(path, parent, sizeof(parent), name, sizeof(name));
        if (err == 0) {
            int pfd = fs_open_in_root(to_rel_path(parent), O_PATH | O_DIRECTORY, 0);
            if (pfd < 0) {
                err = pfd;
            } else {
                if (mkdirat(pfd, name, mode & 07777) < 0)
                    err = -errno;
                close(pfd);
            }
        }
    }
    if (err == 0)
        sync_root_fs();
    ctrl_send_err_reply(SWIPTERMD_OP_FS_MKDIR, req_id, err);
}

static int remove_dir_contents(int dirfd) {
    int dupfd = dup(dirfd);
    if (dupfd < 0)
        return -errno;
    DIR *dir = fdopendir(dupfd);
    if (!dir) {
        int err = -errno;
        close(dupfd);
        return err;
    }

    int err = 0;
    struct dirent *de;
    while (err == 0 && (de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        struct stat st;
        if (fstatat(dirfd, de->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0) {
            err = -errno;
            break;
        }
        if (S_ISDIR(st.st_mode)) {
            int cfd = openat(dirfd, de->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (cfd < 0) {
                err = -errno;
                break;
            }
            err = remove_dir_contents(cfd);
            close(cfd);
            if (err == 0 && unlinkat(dirfd, de->d_name, AT_REMOVEDIR) < 0)
                err = -errno;
        } else {
            if (unlinkat(dirfd, de->d_name, 0) < 0)
                err = -errno;
        }
    }
    closedir(dir);
    return err;
}

void fs_handle_remove(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    bool recursive = swiptermd_r_u8(&r) != 0;
    char path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, path, sizeof(path))) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_REMOVE, req_id, -EINVAL);
        return;
    }

    char norm[PATH_MAX];
    int err = normalize_guest_path(path, norm, sizeof(norm));
    if (err < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_REMOVE, req_id, err);
        return;
    }
    if (strcmp(norm, "/") == 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_REMOVE, req_id, -EBUSY);
        return;
    }

    for (size_t i = 0; i < g_mount_count; i++) {
        if (path_contains(norm, g_mounts[i].guest_path)) {
            ctrl_send_err_reply(SWIPTERMD_OP_FS_REMOVE, req_id, -EBUSY);
            return;
        }
    }

    char parent[PATH_MAX];
    char name[NAME_MAX + 1];
    err = split_parent_name(norm, parent, sizeof(parent), name, sizeof(name));
    if (err < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_REMOVE, req_id, err);
        return;
    }

    int pfd = fs_open_in_root(to_rel_path(parent), O_PATH | O_DIRECTORY, 0);
    if (pfd < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_REMOVE, req_id, pfd);
        return;
    }

    struct stat st;
    if (fstatat(pfd, name, &st, AT_SYMLINK_NOFOLLOW) < 0) {
        err = -errno;
    } else if (!S_ISDIR(st.st_mode)) {
        if (unlinkat(pfd, name, 0) < 0)
            err = -errno;
    } else {
        if (recursive) {
            int dfd = openat(pfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (dfd < 0) {
                err = -errno;
            } else {
                err = remove_dir_contents(dfd);
                close(dfd);
            }
        }
        if (err == 0 && unlinkat(pfd, name, AT_REMOVEDIR) < 0)
            err = -errno;
    }

    close(pfd);
    if (err == 0)
        sync_root_fs();
    ctrl_send_err_reply(SWIPTERMD_OP_FS_REMOVE, req_id, err);
}

static int check_dir_empty(const char *guest_path) {
    int dfd = fs_open_in_root(to_rel_path(guest_path), O_RDONLY | O_DIRECTORY, 0);
    if (dfd < 0)
        return dfd;
    DIR *dir = fdopendir(dfd);
    if (!dir) {
        int err = -errno;
        close(dfd);
        return err;
    }
    bool empty = true;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        empty = false;
        break;
    }
    closedir(dir);
    return empty ? 0 : -ENOTEMPTY;
}

void fs_handle_mount_virtiofs(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    bool read_only = swiptermd_r_u8(&r) != 0;
    (void)swiptermd_r_u8(&r);
    char tag[256];
    char guest_path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, tag, sizeof(tag)) ||
        !swiptermd_r_str_copy(&r, guest_path, sizeof(guest_path))) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_id, -EINVAL);
        return;
    }

    char norm[PATH_MAX];
    int err = normalize_guest_path(guest_path, norm, sizeof(norm));
    if (err < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_id, err);
        return;
    }
    if (strcmp(norm, "/") == 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_id, -EBUSY);
        return;
    }

    for (size_t i = 0; i < g_mount_count; i++) {
        if (strcmp(g_mounts[i].guest_path, norm) == 0) {
            ctrl_send_err_reply(SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_id, -EBUSY);
            return;
        }
    }

    int fd = fs_open_in_root(to_rel_path(norm), O_PATH, 0);
    if (fd == -ENOENT) {
        err = fs_mkdir_p_jailed(norm, 0755);
        fd = err == 0 ? fs_open_in_root(to_rel_path(norm), O_PATH | O_DIRECTORY, 0) : -1;
        if (err == 0 && fd < 0)
            err = fd;
    } else if (fd >= 0) {
        struct stat st;
        if (fstatat(fd, "", &st, AT_EMPTY_PATH) < 0)
            err = -errno;
        else if (!S_ISDIR(st.st_mode))
            err = -ENOTDIR;
        if (err == 0)
            err = check_dir_empty(norm);
    } else {
        err = fd;
    }

    if (err < 0) {
        if (fd >= 0)
            close(fd);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_id, err);
        return;
    }

    char bound_dev[32];
    bind_virtio_fs_slots(bound_dev);

    char target[32];
    snprintf(target, sizeof(target), "/proc/self/fd/%d", fd);
    unsigned long mflags = read_only ? MS_RDONLY : 0;
    err = mount(tag, target, "virtiofs", mflags, NULL) < 0 ? -errno : 0;
    close(fd);
    if (err < 0) {
        if (bound_dev[0] != '\0')
            (void)write_sysfs_str("/sys/bus/platform/drivers/virtio-mmio/unbind", bound_dev);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_id, err);
        return;
    }

    char *dup_path = strdup(norm);
    active_mount_t *grown = dup_path ? realloc(g_mounts, (g_mount_count + 1) * sizeof(*g_mounts)) : NULL;
    if (grown) {
        g_mounts = grown;
        g_mounts[g_mount_count].guest_path = dup_path;
        strcpy(g_mounts[g_mount_count].dev_name, bound_dev);
        g_mount_count++;
    } else {
        free(dup_path);
    }

    ctrl_send_err_reply(SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_id, 0);
}

void fs_unmount_all(void) {
    while (g_mount_count > 0) {
        size_t i = --g_mount_count;
        if (g_mounts[i].guest_path) {
            char full[PATH_MAX];
            if (snprintf(full, sizeof(full), "%s%s", GUEST_ROOT, g_mounts[i].guest_path) < (int)sizeof(full)) {
                if (umount2(full, 0) < 0)
                    (void)umount2(full, MNT_DETACH);
            }
            free(g_mounts[i].guest_path);
        }
        if (g_mounts[i].dev_name[0] != '\0')
            (void)write_sysfs_str("/sys/bus/platform/drivers/virtio-mmio/unbind", g_mounts[i].dev_name);
    }
    free(g_mounts);
    g_mounts = NULL;
}

void fs_handle_unmount(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    char guest_path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, guest_path, sizeof(guest_path))) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_UNMOUNT, req_id, -EINVAL);
        return;
    }

    char norm[PATH_MAX];
    int err = normalize_guest_path(guest_path, norm, sizeof(norm));
    if (err < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_UNMOUNT, req_id, err);
        return;
    }

    ssize_t idx = -1;
    for (size_t i = 0; i < g_mount_count; i++) {
        if (strcmp(g_mounts[i].guest_path, norm) == 0) {
            idx = (ssize_t)i;
            break;
        }
    }
    if (idx < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_UNMOUNT, req_id, -EINVAL);
        return;
    }

    char full[PATH_MAX];
    if (snprintf(full, sizeof(full), "%s%s", GUEST_ROOT, norm) >= (int)sizeof(full)) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_UNMOUNT, req_id, -ENAMETOOLONG);
        return;
    }

    if (umount2(full, 0) < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_UNMOUNT, req_id, -errno);
        return;
    }

    if (g_mounts[idx].dev_name[0] != '\0')
        (void)write_sysfs_str("/sys/bus/platform/drivers/virtio-mmio/unbind", g_mounts[idx].dev_name);

    free(g_mounts[idx].guest_path);
    g_mounts[idx] = g_mounts[--g_mount_count];
    ctrl_send_err_reply(SWIPTERMD_OP_FS_UNMOUNT, req_id, 0);
}

int fs_apply_managed_resolver(void) {
    if (g_daemon.init_mode != SWIPTERMD_INIT_MANAGED)
        return 0;
    if (g_resolver_len == 0)
        return fs_write_file_jailed("/etc/resolv.conf", (const uint8_t *)GUEST_DEFAULT_RESOLV_CONF,
                                    sizeof(GUEST_DEFAULT_RESOLV_CONF) - 1, 0644);
    return fs_write_file_jailed("/etc/resolv.conf", g_resolver, g_resolver_len, 0644);
}

void fs_handle_sync_dns(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    swiptermd_slice_t data = swiptermd_r_bytes(&r);
    if (r.underflow) {
        ctrl_send_err_reply(SWIPTERMD_OP_SYNC_DNS, req_id, -EINVAL);
        return;
    }
    if (data.len > sizeof(g_resolver)) {
        ctrl_send_err_reply(SWIPTERMD_OP_SYNC_DNS, req_id, -E2BIG);
        return;
    }
    memcpy(g_resolver, data.ptr, data.len);
    g_resolver_len = data.len;
    int err = fs_write_file_jailed(GUEST_RUN_HOST "/resolv.conf", g_resolver, g_resolver_len, 0644);
    if (err == 0 && g_daemon.init_started)
        err = fs_apply_managed_resolver();
    ctrl_send_err_reply(SWIPTERMD_OP_SYNC_DNS, req_id, err);
}
