#define _DARWIN_C_SOURCE

#include "host_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct link_map {
    struct link_slot {
        uint32_t ino;
        char *path;
    } *slots;
    size_t cap;
    size_t count;
} link_map_t;

static char **link_map_slot(link_map_t *m, uint32_t ino, bool *found) {
    if ((m->count + 1) * 2 > m->cap) {
        size_t cap = m->cap ? m->cap * 2 : 256;
        struct link_slot *slots = calloc(cap, sizeof(*slots));
        if (!slots) {
            return NULL;
        }
        for (size_t i = 0; i < m->cap; i++) {
            if (m->slots[i].ino != 0) {
                size_t pos = (m->slots[i].ino * 2654435761U) & (cap - 1);
                while (slots[pos].ino != 0) {
                    pos = (pos + 1) & (cap - 1);
                }
                slots[pos] = m->slots[i];
            }
        }
        free(m->slots);
        m->slots = slots;
        m->cap = cap;
    }
    size_t pos = (ino * 2654435761U) & (m->cap - 1);
    while (m->slots[pos].ino != 0 && m->slots[pos].ino != ino) {
        pos = (pos + 1) & (m->cap - 1);
    }
    *found = m->slots[pos].ino == ino;
    if (!*found) {
        m->slots[pos].ino = ino;
        m->count++;
    }
    return &m->slots[pos].path;
}

static void link_map_free(link_map_t *m) {
    for (size_t i = 0; i < m->cap; i++) {
        free(m->slots[i].path);
    }
    free(m->slots);
}

typedef struct export_state {
    struct archive *a;
    ext4_fs_t *fs;
    link_map_t links;
    uint64_t total;
    uint64_t done;
    void *context;
    swipterm_progress_handler progress;
    bool archive_failed;
} export_state_t;

static int report_progress(export_state_t *st) {
    if (!st->progress) {
        return 0;
    }
    double frac = st->total > 0 ? (double)st->done / (double)st->total : 0.0;
    return st->progress(st->context, frac > 0.99 ? 0.99 : frac) ? 0 : -ECANCELED;
}

static int write_archive_chunk(void *ctx, const void *data, size_t len) {
    export_state_t *st = ctx;
    const uint8_t *p = data;
    while (len > 0) {
        la_ssize_t n = archive_write_data(st->a, p, len);
        if (n <= 0) {
            st->archive_failed = true;
            return -EIO;
        }
        p += n;
        len -= (size_t)n;
    }
    return report_progress(st);
}

typedef struct data_map {
    struct data_range {
        uint64_t offset;
        uint64_t length;
    } *ranges;
    size_t count;
    size_t cap;
    uint64_t bytes;
} data_map_t;

static int collect_range(void *ctx, uint64_t offset, uint64_t length) {
    data_map_t *m = ctx;
    m->bytes += length;
    if (m->count > 0 && m->ranges[m->count - 1].offset + m->ranges[m->count - 1].length == offset) {
        m->ranges[m->count - 1].length += length;
        return 0;
    }
    if (m->count == m->cap) {
        size_t cap = m->cap ? m->cap * 2 : 16;
        struct data_range *ranges = realloc(m->ranges, cap * sizeof(*ranges));
        if (!ranges) {
            return -ENOMEM;
        }
        m->ranges = ranges;
        m->cap = cap;
    }
    m->ranges[m->count++] = (struct data_range){.offset = offset, .length = length};
    return 0;
}

static int describe_holes(export_state_t *st, const ext4_node_t *node, struct archive_entry *entry) {
    data_map_t map = {0};
    int rc = ext4_fs_map(st->fs, node, collect_range, &map);
    if (rc == 0 && map.bytes < node->size) {
        for (size_t i = 0; i < map.count; i++) {
            uint64_t end = map.ranges[i].offset + map.ranges[i].length;
            uint64_t length = end > node->size ? node->size - map.ranges[i].offset : map.ranges[i].length;
            archive_entry_sparse_add_entry(entry, (la_int64_t)map.ranges[i].offset, (la_int64_t)length);
        }
        if (map.count == 0 || map.ranges[map.count - 1].offset + map.ranges[map.count - 1].length < node->size) {
            archive_entry_sparse_add_entry(entry, (la_int64_t)node->size, 0);
        }
    }
    free(map.ranges);
    return rc;
}

static uint16_t get_le16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int add_posix_acl(struct archive_entry *entry, int type, const ext4_xattr_info_t *xattr) {
    const uint8_t *value = xattr->value;
    if (xattr->size < 4 || get_le32(value) != 2 || (xattr->size - 4) % 8 != 0) {
        return -EIO;
    }
    for (size_t off = 4; off < xattr->size; off += 8) {
        int tag;
        switch (get_le16(value + off)) {
        case 0x01: tag = ARCHIVE_ENTRY_ACL_USER_OBJ; break;
        case 0x02: tag = ARCHIVE_ENTRY_ACL_USER; break;
        case 0x04: tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ; break;
        case 0x08: tag = ARCHIVE_ENTRY_ACL_GROUP; break;
        case 0x10: tag = ARCHIVE_ENTRY_ACL_MASK; break;
        case 0x20: tag = ARCHIVE_ENTRY_ACL_OTHER; break;
        default: return -EIO;
        }
        uint16_t perm = get_le16(value + off + 2);
        int permset = ((perm & 4) ? ARCHIVE_ENTRY_ACL_READ : 0) | ((perm & 2) ? ARCHIVE_ENTRY_ACL_WRITE : 0) |
                      ((perm & 1) ? ARCHIVE_ENTRY_ACL_EXECUTE : 0);
        int qual = tag == ARCHIVE_ENTRY_ACL_USER || tag == ARCHIVE_ENTRY_ACL_GROUP ? (int)get_le32(value + off + 4) : -1;
        if (archive_entry_acl_add_entry(entry, type, permset, tag, qual, NULL) != ARCHIVE_OK) {
            return -EIO;
        }
    }
    return 0;
}

static int export_entry(void *ctx, const char *path, const ext4_entry_info_t *info) {
    export_state_t *st = ctx;
    const ext4_node_t *node = &info->node;
    if (S_ISSOCK(node->mode)) {
        return 0;
    }
    struct archive_entry *entry = archive_entry_new();
    if (!entry) {
        return -ENOMEM;
    }
    archive_entry_set_pathname(entry, path);
    archive_entry_set_mode(entry, (mode_t)node->mode);
    archive_entry_set_uid(entry, node->uid);
    archive_entry_set_gid(entry, node->gid);
    archive_entry_set_mtime(entry, (time_t)info->mtime, (long)info->mtime_nsec);
    for (size_t i = 0; i < info->xattr_count; i++) {
        const ext4_xattr_info_t *xattr = &info->xattrs[i];
        int rc = 0;
        if (strcmp(xattr->name, "system.posix_acl_access") == 0) {
            rc = add_posix_acl(entry, ARCHIVE_ENTRY_ACL_TYPE_ACCESS, xattr);
        } else if (strcmp(xattr->name, "system.posix_acl_default") == 0) {
            rc = add_posix_acl(entry, ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, xattr);
        } else {
            archive_entry_xattr_add_entry(entry, xattr->name, xattr->value, xattr->size);
        }
        if (rc < 0) {
            archive_entry_free(entry);
            return rc;
        }
    }

    bool hardlink = false;
    if (!S_ISDIR(node->mode) && node->nlink > 1) {
        bool found = false;
        char **slot = link_map_slot(&st->links, node->ino, &found);
        if (!slot) {
            archive_entry_free(entry);
            return -ENOMEM;
        }
        if (found) {
            archive_entry_set_hardlink(entry, *slot);
            archive_entry_set_size(entry, 0);
            hardlink = true;
        } else if (!(*slot = strdup(path))) {
            archive_entry_free(entry);
            return -ENOMEM;
        }
    }
    if (!hardlink) {
        if (S_ISREG(node->mode)) {
            archive_entry_set_size(entry, (la_int64_t)node->size);
            int rc = node->size > 0 ? describe_holes(st, node, entry) : 0;
            if (rc < 0) {
                archive_entry_free(entry);
                return rc;
            }
        } else if (S_ISLNK(node->mode)) {
            archive_entry_set_symlink(entry, info->symlink_target ? info->symlink_target : "");
        } else if (S_ISCHR(node->mode) || S_ISBLK(node->mode)) {
            archive_entry_set_rdevmajor(entry, (unsigned int)info->rdev_major);
            archive_entry_set_rdevminor(entry, (unsigned int)info->rdev_minor);
        }
    }
    int r = archive_write_header(st->a, entry);
    archive_entry_free(entry);
    if (r != ARCHIVE_OK && r != ARCHIVE_WARN) {
        st->archive_failed = true;
        return -EIO;
    }
    if (!hardlink && S_ISREG(node->mode) && node->size > 0) {
        int rc = ext4_fs_stream(st->fs, node, 0, node->size, write_archive_chunk, st);
        if (rc < 0) {
            return rc;
        }
    }
    st->done++;
    return report_progress(st);
}

static void set_error(char **error_message, const char *what, const char *detail) {
    if (error_message && !*error_message) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s: %s", what, detail ? detail : "unknown error");
        *error_message = strdup(buf);
    }
}

bool swipterm_root_export(const char *root_path, const char *archive_path,
                          void *context, swipterm_progress_handler progress, char **error_message) {
    if (error_message) {
        *error_message = NULL;
    }
    if (!root_path || !archive_path) {
        set_error(error_message, "invalid arguments", NULL);
        return false;
    }
    char disk_path[1024];
    if (snprintf(disk_path, sizeof(disk_path), "%s/root.ext4", root_path) >= (int)sizeof(disk_path)) {
        set_error(error_message, "invalid root path", strerror(ENAMETOOLONG));
        return false;
    }
    char partial_path[1024];
    if (snprintf(partial_path, sizeof(partial_path), "%s.partial.XXXXXX", archive_path) >= (int)sizeof(partial_path)) {
        set_error(error_message, "invalid archive path", strerror(ENAMETOOLONG));
        return false;
    }
    int root_fd = open(root_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (root_fd < 0) {
        set_error(error_message, "failed to open root", strerror(errno));
        return false;
    }
    if (flock(root_fd, LOCK_SH | LOCK_NB) != 0) {
        set_error(error_message, "failed to lock root",
                  errno == EWOULDBLOCK ? "the root is used by a running machine" : strerror(errno));
        close(root_fd);
        return false;
    }
    int disk_fd = open(disk_path, O_RDONLY | O_CLOEXEC);
    if (disk_fd < 0) {
        set_error(error_message, "failed to open root disk", strerror(errno));
        close(root_fd);
        return false;
    }

    export_state_t st = {.context = context, .progress = progress};
    bool ok = false;
    int archive_fd = -1;
    int rc = ext4_fs_open(disk_fd, &st.fs);
    if (rc < 0) {
        set_error(error_message, "failed to read root disk", strerror(-rc));
        goto out;
    }
    st.total = ext4_fs_used_inodes(st.fs);
    st.a = archive_write_new();
    if (!st.a) {
        set_error(error_message, "failed to allocate archive writer", NULL);
        goto out;
    }
    if (archive_write_add_filter_gzip(st.a) != ARCHIVE_OK ||
        archive_write_set_format_pax_restricted(st.a) != ARCHIVE_OK) {
        set_error(error_message, "failed to create archive", archive_error_string(st.a));
        goto out;
    }
    bool staged = true;
    archive_fd = mkostemp(partial_path, O_CLOEXEC);
    if (archive_fd >= 0) {
        (void)fchmod(archive_fd, 0644);
    } else if (errno == EPERM || errno == EACCES) {
        staged = false;
        archive_fd = open(archive_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    }
    if (archive_fd < 0) {
        set_error(error_message, "failed to create archive", strerror(errno));
        goto out;
    }
    if (archive_write_open_fd(st.a, archive_fd) != ARCHIVE_OK) {
        set_error(error_message, "failed to create archive", archive_error_string(st.a));
    } else if ((rc = ext4_fs_walk(st.fs, export_entry, &st)) != 0) {
        set_error(error_message, "failed to export root", st.archive_failed ? archive_error_string(st.a) : strerror(-rc));
        archive_write_fail(st.a);
    } else if (archive_write_close(st.a) != ARCHIVE_OK) {
        set_error(error_message, "failed to write archive", archive_error_string(st.a));
    } else if ((rc = report_progress(&st)) != 0) {
        set_error(error_message, "failed to export root", strerror(-rc));
    } else if (fsync(archive_fd) != 0) {
        set_error(error_message, "failed to write archive", strerror(errno));
    } else {
        int closed = close(archive_fd);
        archive_fd = -1;
        if (closed != 0 || (staged && rename(partial_path, archive_path) != 0)) {
            set_error(error_message, "failed to write archive", strerror(errno));
        } else {
            ok = true;
        }
    }
    if (!ok) {
        unlink(staged ? partial_path : archive_path);
    } else if (progress) {
        progress(context, 1.0);
    }

out:
    if (st.a) {
        archive_write_free(st.a);
    }
    if (archive_fd >= 0) {
        close(archive_fd);
    }
    link_map_free(&st.links);
    ext4_fs_close(st.fs);
    close(disk_fd);
    close(root_fd);
    return ok;
}
