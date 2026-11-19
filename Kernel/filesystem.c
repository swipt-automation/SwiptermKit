#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "internal.h"

#include "fs/path.h"
#define ISH_INTERNAL
#include "fs/fake.h"

extern struct mount *g_fakefs_mount;

struct bind_mount {
    char *path;
    bool directory;
};

static struct bind_mount *bind_mounts;
static size_t bind_mount_count;

static void stat_from_statbuf(const struct statbuf *stat, swipterm_stat *out) {
    *out = (swipterm_stat) {
        .mode = stat->mode,
        .uid = stat->uid,
        .gid = stat->gid,
        .nlink = stat->nlink,
        .size = stat->size,
        .inode = stat->inode,
        .rdev = stat->rdev,
        .access_time = {stat->atime, stat->atime_nsec},
        .modification_time = {stat->mtime, stat->mtime_nsec},
        .change_time = {stat->ctime, stat->ctime_nsec},
    };
}

int swipterm_stat_path(const char *path, bool follow_symlinks, swipterm_stat *stat_out) {
    struct statbuf stat;
    host_enter();
    int err = generic_statat(AT_PWD, path, &stat, follow_symlinks);
    host_leave();
    if (err == 0)
        stat_from_statbuf(&stat, stat_out);
    return err;
}

int filesystem_read_file(const char *path, void **data_out, size_t *length_out) {
    struct fd *fd = generic_open(path, O_RDONLY_, 0);
    if (IS_ERR(fd))
        return (int) PTR_ERR(fd);
    int err = 0;
    if (S_ISDIR(fd->stat.mode) || S_ISDIR(fd->type))
        err = _EISDIR;
    else if (fd->ops->read == NULL)
        err = _EINVAL;
    size_t capacity = 4096;
    size_t length = 0;
    char *data = err == 0 ? malloc(capacity) : NULL;
    if (err == 0 && data == NULL)
        err = _ENOMEM;
    while (err == 0) {
        if (length == capacity) {
            char *grown = realloc(data, capacity * 2);
            if (grown == NULL) {
                err = _ENOMEM;
                break;
            }
            data = grown;
            capacity *= 2;
        }
        ssize_t count = fd->ops->read(fd, data + length, capacity - length);
        if (count < 0)
            err = (int) count;
        else if (count == 0)
            break;
        else
            length += count;
    }
    fd_close(fd);
    if (err < 0) {
        free(data);
        return err;
    }
    *data_out = data;
    *length_out = length;
    return 0;
}

int swipterm_read_file(const char *path, void **data_out, size_t *length_out) {
    host_enter();
    int err = filesystem_read_file(path, data_out, length_out);
    host_leave();
    return err;
}

int filesystem_create_directory(const char *path, uint32_t mode, bool intermediates) {
    if (!intermediates)
        return generic_mkdirat(AT_PWD, path, mode);
    char normalized[MAX_PATH];
    int err = path_normalize(AT_PWD, path, normalized, N_SYMLINK_FOLLOW);
    if (err < 0)
        return err;
    size_t length = strlen(normalized);
    for (size_t end = 1; end <= length; end++) {
        if (end < length && normalized[end] != '/')
            continue;
        char saved = normalized[end];
        normalized[end] = '\0';
        err = generic_mkdirat(AT_PWD, normalized, mode);
        if (err == _EEXIST) {
            struct statbuf stat;
            err = generic_statat(AT_PWD, normalized, &stat, true);
            if (err == 0 && !S_ISDIR(stat.mode))
                err = _ENOTDIR;
        }
        normalized[end] = saved;
        if (err < 0)
            return err;
    }
    return 0;
}

int swipterm_create_directory(const char *path, uint32_t mode, bool intermediates) {
    host_enter();
    int err = filesystem_create_directory(path, mode, intermediates);
    host_leave();
    return err;
}

static int split_parent(const char *path, char *parent, char *name) {
    int err = path_normalize(AT_PWD, path, parent, N_SYMLINK_NOFOLLOW);
    if (err < 0)
        return err;
    char *slash = strrchr(parent, '/');
    if (slash == NULL || slash[1] == '\0')
        return _EISDIR;
    strlcpy(name, slash + 1, MAX_NAME);
    *slash = '\0';
    return 0;
}

int filesystem_write_file(const char *path, const void *data, size_t length, uint32_t mode) {
    char parent[MAX_PATH];
    char name[MAX_NAME];
    int err = split_parent(path, parent, name);
    if (err < 0)
        return err;
    if (parent[0] != '\0') {
        err = filesystem_create_directory(parent, 0755, true);
        if (err < 0)
            return err;
    }

    char temporary[MAX_PATH];
    if (snprintf(temporary, sizeof(temporary), "%s/.%s.%08x", parent, name, arc4random()) >= (int) sizeof(temporary))
        return _ENAMETOOLONG;
    struct fd *fd = generic_open(temporary, O_WRONLY_ | O_CREAT_ | O_EXCL_, mode & 07777);
    if (IS_ERR(fd))
        return (int) PTR_ERR(fd);
    const char *bytes = data;
    size_t written = 0;
    while (err == 0 && written < length) {
        ssize_t count = fd->ops->write(fd, bytes + written, length - written);
        if (count < 0)
            err = (int) count;
        else
            written += count;
    }
    if (err == 0 && fd->ops->fsync != NULL)
        err = fd->ops->fsync(fd);
    fd_close(fd);
    if (err == 0)
        err = generic_renameat(AT_PWD, temporary, AT_PWD, path);
    if (err < 0)
        generic_unlinkat(AT_PWD, temporary);
    return err;
}

int swipterm_write_file(const char *path, const void *data, size_t length, uint32_t mode) {
    host_enter();
    int err = filesystem_write_file(path, data, length, mode);
    host_leave();
    return err;
}

static int list_directory(const char *path, swipterm_directory_entry **entries_out, size_t *count_out) {
    struct fd *fd = generic_open(path, O_RDONLY_ | O_DIRECTORY_, 0);
    if (IS_ERR(fd))
        return (int) PTR_ERR(fd);
    if (fd->ops->readdir == NULL) {
        fd_close(fd);
        return _ENOTDIR;
    }
    swipterm_directory_entry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int err;
    struct dir_entry entry;
    while ((err = fd->ops->readdir(fd, &entry)) > 0) {
        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0)
            continue;
        if (count == capacity) {
            capacity = capacity == 0 ? 16 : capacity * 2;
            swipterm_directory_entry *grown = realloc(entries, capacity * sizeof(*entries));
            if (grown == NULL) {
                err = _ENOMEM;
                break;
            }
            entries = grown;
        }
        struct statbuf stat = {};
        generic_statat(fd, entry.name, &stat, false);
        entries[count].name = strdup(entry.name);
        entries[count].mode = stat.mode;
        count++;
    }
    fd_close(fd);
    if (err < 0) {
        swipterm_free_directory_entries(entries, count);
        return err;
    }
    *entries_out = entries;
    *count_out = count;
    return 0;
}

int swipterm_list_directory(const char *path, swipterm_directory_entry **entries_out, size_t *count_out) {
    host_enter();
    int err = list_directory(path, entries_out, count_out);
    host_leave();
    return err;
}

void swipterm_free_directory_entries(swipterm_directory_entry *entries, size_t count) {
    for (size_t i = 0; i < count; i++)
        free(entries[i].name);
    free(entries);
}

static bool path_contains(const char *ancestor, const char *path) {
    size_t length = strlen(ancestor);
    return strncmp(ancestor, path, length) == 0 && (path[length] == '/' || path[length] == '\0');
}

static int remove_item(const char *path, bool recursive) {
    struct statbuf stat;
    int err = generic_statat(AT_PWD, path, &stat, false);
    if (err < 0)
        return err;
    if (!S_ISDIR(stat.mode))
        return generic_unlinkat(AT_PWD, path);
    if (recursive) {
        swipterm_directory_entry *entries;
        size_t count;
        err = list_directory(path, &entries, &count);
        if (err < 0)
            return err;
        for (size_t i = 0; i < count && err == 0; i++) {
            char child[MAX_PATH];
            if (snprintf(child, sizeof(child), "%s/%s", path, entries[i].name) >= (int) sizeof(child))
                err = _ENAMETOOLONG;
            else
                err = remove_item(child, true);
        }
        swipterm_free_directory_entries(entries, count);
        if (err < 0)
            return err;
    }
    return generic_rmdirat(AT_PWD, path);
}

int swipterm_remove(const char *path, bool recursive) {
    char normalized[MAX_PATH];
    host_enter();
    int err = path_normalize(AT_PWD, path, normalized, N_SYMLINK_NOFOLLOW);
    for (size_t i = 0; err == 0 && i < bind_mount_count; i++) {
        if (path_contains(normalized, bind_mounts[i].path))
            err = _EBUSY;
    }
    if (err == 0)
        err = remove_item(normalized, recursive);
    host_leave();
    return err;
}

static int save_bind_mounts(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/bind-mounts", root_path);
    if (bind_mount_count == 0)
        return unlink(path) < 0 && errno != ENOENT ? errno_map() : 0;
    char temporary[PATH_MAX];
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    FILE *file = fopen(temporary, "w");
    if (file == NULL)
        return errno_map();
    for (size_t i = 0; i < bind_mount_count; i++)
        fprintf(file, "%c %s\n", bind_mounts[i].directory ? 'd' : 'f', bind_mounts[i].path);
    if (fclose(file) != 0 || rename(temporary, path) < 0)
        return errno_map();
    return 0;
}

static int restore_mount_point(const char *path, bool directory) {
    int root_fd = g_fakefs_mount->root_fd;
    const char *relative = fix_path(path);
    struct stat host_stat;
    if (fstatat(root_fd, relative, &host_stat, AT_SYMLINK_NOFOLLOW) == 0) {
        if (!S_ISLNK(host_stat.st_mode))
            return 0;
        if (unlinkat(root_fd, relative, 0) < 0)
            return errno_map();
    }
    if (directory) {
        if (mkdirat(root_fd, relative, 0755) < 0 && errno != EEXIST)
            return errno_map();
    } else {
        int fd = openat(root_fd, relative, O_WRONLY | O_CREAT, 0644);
        if (fd < 0)
            return errno_map();
        close(fd);
    }
    struct attr attr = {.type = attr_mode, .mode = directory ? 0755 : 0644};
    return generic_setattrat(AT_PWD, path, attr, false);
}

int filesystem_boot(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/bind-mounts", root_path);
    FILE *file = fopen(path, "r");
    if (file == NULL)
        return 0;
    char line[MAX_PATH + 4];
    int err = 0;
    while (err == 0 && fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) > 2)
            err = restore_mount_point(line + 2, line[0] == 'd');
    }
    fclose(file);
    if (err == 0 && unlink(path) < 0)
        err = errno_map();
    return err;
}

static struct bind_mount *find_bind_mount(const char *path) {
    for (size_t i = 0; i < bind_mount_count; i++) {
        if (strcmp(bind_mounts[i].path, path) == 0)
            return &bind_mounts[i];
    }
    return NULL;
}

static int create_mount_point(const char *path, bool directory) {
    if (directory)
        return filesystem_create_directory(path, 0755, true);
    char parent[MAX_PATH];
    char name[MAX_NAME];
    int err = split_parent(path, parent, name);
    if (err == 0 && parent[0] != '\0')
        err = filesystem_create_directory(parent, 0755, true);
    if (err < 0)
        return err;
    struct fd *fd = generic_open(path, O_WRONLY_ | O_CREAT_ | O_EXCL_, 0644);
    if (IS_ERR(fd))
        return (int) PTR_ERR(fd);
    fd_close(fd);
    return 0;
}

static int check_mount_point(const char *path, bool directory) {
    struct statbuf stat;
    int err = generic_statat(AT_PWD, path, &stat, true);
    if (err < 0)
        return err;
    if (directory != S_ISDIR(stat.mode))
        return directory ? _ENOTDIR : _EISDIR;
    if (!directory)
        return S_ISREG(stat.mode) && stat.size == 0 ? 0 : _EEXIST;
    swipterm_directory_entry *entries;
    size_t count;
    err = list_directory(path, &entries, &count);
    if (err < 0)
        return err;
    swipterm_free_directory_entries(entries, count);
    return count == 0 ? 0 : _ENOTEMPTY;
}

static int prepare_mount_point(const char *guest_path, bool directory, char *point) {
    int err = path_normalize(AT_PWD, guest_path, point, N_SYMLINK_FOLLOW);
    if (err < 0)
        return err;
    if (point[0] == '\0')
        return _EBUSY;
    struct mount *mount = mount_find(point);
    bool on_root = mount == g_fakefs_mount;
    mount_release(mount);
    if (!on_root)
        return _EINVAL;
    struct bind_mount *existing = find_bind_mount(point);
    if (existing != NULL)
        return existing->directory == directory ? 0 : _EBUSY;

    struct statbuf stat;
    err = generic_statat(AT_PWD, point, &stat, false);
    if (err == 0) {
        struct stat host_stat;
        if (fstatat(g_fakefs_mount->root_fd, fix_path(point), &host_stat, AT_SYMLINK_NOFOLLOW) == 0 &&
            S_ISLNK(host_stat.st_mode))
            err = restore_mount_point(point, S_ISDIR(stat.mode));
        if (err == 0)
            err = check_mount_point(point, directory);
    } else if (err == _ENOENT) {
        err = create_mount_point(point, directory);
    }
    return err;
}

int swipterm_mount(const char *host_path, const char *guest_path, bool read_only) {
    struct stat host_stat;
    if (stat(host_path, &host_stat) < 0)
        return errno_map();
    bool directory = S_ISDIR(host_stat.st_mode);
    if (!directory && !S_ISREG(host_stat.st_mode))
        return _EINVAL;

    char point[MAX_PATH];
    host_enter();
    int err = prepare_mount_point(guest_path, directory, point);
    if (err == 0)
        err = fakefs_bind_mount(point, host_path, read_only);
    if (err == 0 && find_bind_mount(point) == NULL) {
        struct bind_mount *grown = realloc(bind_mounts, (bind_mount_count + 1) * sizeof(*bind_mounts));
        if (grown == NULL) {
            err = _ENOMEM;
        } else {
            bind_mounts = grown;
            bind_mounts[bind_mount_count++] = (struct bind_mount) {.path = strdup(point), .directory = directory};
            err = save_bind_mounts();
        }
    }
    host_leave();
    return err;
}

int swipterm_unmount(const char *guest_path) {
    char point[MAX_PATH];
    host_enter();
    int err = path_normalize(AT_PWD, guest_path, point, N_SYMLINK_FOLLOW);
    struct bind_mount *mount = err == 0 ? find_bind_mount(point) : NULL;
    if (err == 0 && mount == NULL)
        err = _EINVAL;
    if (err == 0)
        err = fakefs_bind_unmount(point);
    if (err == 0) {
        err = restore_mount_point(point, mount->directory);
        free(mount->path);
        *mount = bind_mounts[--bind_mount_count];
        int save_err = save_bind_mounts();
        if (err == 0)
            err = save_err;
    }
    host_leave();
    return err;
}
