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

enum {
    ROOT_FILE_MAX = 64 * 1024,
};

typedef struct active_mount {
    char *guest_path;
    unsigned mmio_addr;
} active_mount_t;

static active_mount_t *g_mounts;
static size_t g_mount_count;

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

char *fs_read_root_file(const char *rel, struct stat *st) {
    int fd = fs_open_in_root(rel, O_RDONLY | O_NOFOLLOW, 0);
    if (fd < 0)
        return NULL;
    char *data = NULL;
    if (fstat(fd, st) == 0 && S_ISREG(st->st_mode) && st->st_size <= ROOT_FILE_MAX &&
        (data = malloc((size_t)st->st_size + 1))) {
        if (read_all_blocking(fd, data, (size_t)st->st_size) == 0) {
            data[st->st_size] = '\0';
        } else {
            free(data);
            data = NULL;
        }
    }
    close(fd);
    return data;
}

int fs_normalize_path(const char *in, char *out, size_t cap) {
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
    int err = fs_normalize_path(guest_path, norm, sizeof(norm));
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

int fs_mkdir_p_jailed(const char *guest_path, uint32_t mode) {
    char norm[PATH_MAX];
    int err = fs_normalize_path(guest_path, norm, sizeof(norm));
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

int fs_write_file_jailed(const char *guest_path, const uint8_t *data, size_t len, uint32_t mode, bool sync) {
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
    if (err == 0 && sync && fsync(fd) < 0)
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
    swiptermd_buf_r_t r = {.data = body, .len = len};
    char path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, path, sizeof(path))) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_STAT, req_id, -EINVAL);
        return;
    }

    int fd = fs_open_in_root(to_rel_path(path), O_PATH, 0);
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

    uint8_t out[SWIPTERMD_ERR_REPLY_SIZE + SWIPTERMD_U32_MSG_SIZE];
    swiptermd_buf_w_t w = {.data = out, .cap = sizeof(out)};
    swiptermd_w_u32(&w, 0);
    swiptermd_w_u32(&w, st.st_mode);
    ctrl_send_frame((uint16_t)(SWIPTERMD_OP_FS_STAT | SWIPTERMD_OP_REPLY_FLAG),
                    req_id, out, (uint32_t)w.len);
}

void fs_handle_read(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = {.data = body, .len = len};
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

    int fd = fs_open_in_root(to_rel_path(path), O_RDONLY | O_NONBLOCK, 0);
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
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_READ, req_id, -EINVAL);
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
    swiptermd_buf_r_t r = {.data = body, .len = len};
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
            if (err == 0 && (flags & SWIPTERMD_FS_WRITE_FLAG_SYNC) && fsync(fd) < 0)
                err = -errno;
            close(fd);
        }
    } else {
        err = fs_write_file_jailed(path, data.ptr, data.len, mode, flags & SWIPTERMD_FS_WRITE_FLAG_SYNC);
    }
    ctrl_send_err_reply(SWIPTERMD_OP_FS_WRITE, req_id, err);
}

void fs_handle_mkdir(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = {.data = body, .len = len};
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
    ctrl_send_err_reply(SWIPTERMD_OP_FS_MKDIR, req_id, err);
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

int fs_prepare_mountpoint(const char *guest_path, char norm[PATH_MAX]) {
    int err = fs_normalize_path(guest_path, norm, PATH_MAX);
    if (err < 0)
        return err;
    if (strcmp(norm, "/") == 0)
        return -EBUSY;
    for (size_t i = 0; i < g_mount_count; i++) {
        if (strcmp(g_mounts[i].guest_path, norm) == 0)
            return -EBUSY;
    }

    int fd = fs_open_in_root(to_rel_path(norm), O_PATH, 0);
    if (fd == -ENOENT) {
        err = fs_mkdir_p_jailed(norm, 0755);
        return err < 0 ? err : fs_open_in_root(to_rel_path(norm), O_PATH | O_DIRECTORY, 0);
    }
    if (fd < 0)
        return fd;
    struct stat st;
    if (fstatat(fd, "", &st, AT_EMPTY_PATH) < 0)
        err = -errno;
    else if (!S_ISDIR(st.st_mode))
        err = -ENOTDIR;
    else
        err = check_dir_empty(norm);
    if (err < 0) {
        close(fd);
        return err;
    }
    return fd;
}

void fs_handle_mount_virtiofs(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = {.data = body, .len = len};
    bool read_only = swiptermd_r_u8(&r) != 0;
    char tag[256];
    char guest_path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, tag, sizeof(tag)) ||
        !swiptermd_r_str_copy(&r, guest_path, sizeof(guest_path))) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_id, -EINVAL);
        return;
    }

    char norm[PATH_MAX];
    int fd = fs_prepare_mountpoint(guest_path, norm);
    if (fd < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_id, fd);
        return;
    }

    unsigned bound_addr = 0;
    for (unsigned slot = VIRTIO_SLOT_FS_BASE; slot < VIRTIO_SLOT_FS_BASE + VIRTIO_SLOT_FS_COUNT; slot++) {
        if (virtio_mmio_bind(VIRTIO_MMIO_ADDR(slot)) > 0 && bound_addr == 0)
            bound_addr = VIRTIO_MMIO_ADDR(slot);
    }

    char target[32];
    snprintf(target, sizeof(target), "/proc/self/fd/%d", fd);
    unsigned long mflags = read_only ? MS_RDONLY : 0;
    int err = mount(tag, target, "virtiofs", mflags, NULL) < 0 ? -errno : 0;
    close(fd);
    if (err < 0) {
        if (bound_addr != 0)
            virtio_mmio_unbind(bound_addr);
        ctrl_send_err_reply(SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_id, err);
        return;
    }

    char *dup_path = strdup(norm);
    active_mount_t *grown = dup_path ? realloc(g_mounts, (g_mount_count + 1) * sizeof(*g_mounts)) : NULL;
    if (grown) {
        g_mounts = grown;
        g_mounts[g_mount_count].guest_path = dup_path;
        g_mounts[g_mount_count].mmio_addr = bound_addr;
        g_mount_count++;
    } else {
        free(dup_path);
    }

    ctrl_send_err_reply(SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_id, 0);
}

void fs_unmount_all(void) {
    while (g_mount_count > 0) {
        size_t i = --g_mount_count;
        char full[PATH_MAX];
        if (snprintf(full, sizeof(full), "%s%s", GUEST_ROOT, g_mounts[i].guest_path) < (int)sizeof(full) &&
            umount2(full, 0) < 0)
            (void)umount2(full, MNT_DETACH);
        free(g_mounts[i].guest_path);
        if (g_mounts[i].mmio_addr != 0)
            virtio_mmio_unbind(g_mounts[i].mmio_addr);
    }
    free(g_mounts);
    g_mounts = NULL;
}
