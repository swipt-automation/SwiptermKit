#include "swiptermd.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mntent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#define UPPER_DEVICE "/dev/vda"
#define UPPER_MOUNT "/mnt/upper"

enum {
    EXT4_PROBE_SIZE = 4096,
    EXT4_SUPER_OFFSET = 1024,
    EXT4_SUPER_MAGIC = 0xEF53,
    EXT4_BACKUP_SUPER_BLOCK = 32768,
    EXT4_BACKUP_BLOCK_SIZE = 4096,
    EXT4_FEATURE_COMPAT_HAS_JOURNAL = 0x0004,
    EXT4_FEATURE_INCOMPAT_64BIT = 0x0080,
    E2FSCK_UNCORRECTED = 4,
    JOURNAL_RESERVE_BLOCKS = 256,
};

static const unsigned journal_sizes_mib[] = {64, 32, 16, 8, 4};

typedef enum upper_state {
    UPPER_BLANK,
    UPPER_EXT4,
    UPPER_EXT4_BACKUP_ONLY,
} upper_state_t;

typedef struct upper_info {
    upper_state_t state;
    bool has_journal;
    uint32_t block_size;
    uint64_t free_blocks;
} upper_info_t;

static uint16_t get_le16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void decode_super(const uint8_t *sb, upper_state_t state, upper_info_t *info) {
    uint32_t log_block_size = get_le32(sb + 0x18);
    info->state = state;
    info->has_journal = (get_le32(sb + 0x5C) & EXT4_FEATURE_COMPAT_HAS_JOURNAL) != 0;
    info->block_size = log_block_size <= 6 ? 1024U << log_block_size : 0;
    info->free_blocks = get_le32(sb + 0x0C);
    if (get_le32(sb + 0x60) & EXT4_FEATURE_INCOMPAT_64BIT)
        info->free_blocks |= (uint64_t)get_le32(sb + 0x158) << 32;
}

static int probe_upper(upper_info_t *info) {
    int fd = open(UPPER_DEVICE, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    uint8_t block[EXT4_PROBE_SIZE];
    uint8_t backup[1024];
    int rc = 0;
    ssize_t n = pread(fd, block, sizeof(block), 0);
    if (n != (ssize_t)sizeof(block)) {
        rc = n < 0 ? -errno : -EIO;
    } else if (get_le16(block + EXT4_SUPER_OFFSET + 0x38) == EXT4_SUPER_MAGIC) {
        decode_super(block + EXT4_SUPER_OFFSET, UPPER_EXT4, info);
    } else if (pread(fd, backup, sizeof(backup), (off_t)EXT4_BACKUP_SUPER_BLOCK * EXT4_BACKUP_BLOCK_SIZE) ==
                   (ssize_t)sizeof(backup) &&
               get_le16(backup + 0x38) == EXT4_SUPER_MAGIC) {
        decode_super(backup, UPPER_EXT4_BACKUP_ONLY, info);
    } else {
        *info = (upper_info_t){.state = UPPER_BLANK};
        for (size_t i = 0; i < sizeof(block); i++) {
            if (block[i] != 0) {
                rc = -EUCLEAN;
                break;
            }
        }
    }
    close(fd);
    return rc;
}

static int run_program(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0)
        return -errno;
    if (pid == 0) {
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);
        execv(argv[0], argv);
        _exit(127);
    }
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -errno;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

static int check_upper(bool from_backup) {
    char backup_block[16];
    snprintf(backup_block, sizeof(backup_block), "%d", EXT4_BACKUP_SUPER_BLOCK);
    char *const preen[] = {"/sbin/e2fsck", "-p", UPPER_DEVICE, NULL};
    char *const preen_backup[] = {"/sbin/e2fsck", "-p", "-b", backup_block, UPPER_DEVICE, NULL};
    int status = run_program(from_backup ? preen_backup : preen);
    if (status >= 0 && (status & E2FSCK_UNCORRECTED)) {
        boot_err_log("e2fsck -p", status);
        char *const repair[] = {"/sbin/e2fsck", "-f", "-y", UPPER_DEVICE, NULL};
        char *const repair_backup[] = {"/sbin/e2fsck", "-f", "-y", "-b", backup_block, UPPER_DEVICE, NULL};
        status = run_program(from_backup ? repair_backup : repair);
    }
    if (status < 0)
        return status;
    if (status >= E2FSCK_UNCORRECTED) {
        boot_err_log("e2fsck", status);
        return -EUCLEAN;
    }
    return 0;
}

static int add_journal(void) {
    for (size_t i = 0; i < sizeof(journal_sizes_mib) / sizeof(journal_sizes_mib[0]); i++) {
        upper_info_t info;
        int rc = probe_upper(&info);
        if (rc < 0)
            return rc;
        if (info.state != UPPER_EXT4 || info.block_size == 0)
            return -EUCLEAN;
        if (info.has_journal)
            return 0;
        uint64_t journal_blocks = (uint64_t)journal_sizes_mib[i] * 1024 * 1024 / info.block_size;
        if (journal_blocks + JOURNAL_RESERVE_BLOCKS > info.free_blocks)
            continue;
        char size[32];
        snprintf(size, sizeof(size), "size=%u", journal_sizes_mib[i]);
        char *const tune[] = {"/sbin/tune2fs", "-j", "-J", size, UPPER_DEVICE, NULL};
        int status = run_program(tune);
        if (status == 0) {
            if (i > 0)
                console_log("swiptermd: added a %u MiB journal to upper.ext4\n", journal_sizes_mib[i]);
            return 0;
        }
        boot_err_log("tune2fs -j", status);
        rc = check_upper(false);
        if (rc < 0)
            return rc;
    }
    console_log("swiptermd: upper.ext4 has too little free space for a journal\n");
    return -ENOSPC;
}

static int prepare_upper(void) {
    upper_info_t info;
    int rc = probe_upper(&info);
    if (rc < 0)
        return rc;
    if (info.state == UPPER_BLANK) {
        char *const mkfs[] = {
            "/sbin/mke2fs", "-q", "-F", "-t", "ext4", "-b", "4096", "-m", "0",
            "-L", "swipterm-upper", "-J", "size=64",
            "-E", "nodiscard,assume_storage_prezeroed=1", UPPER_DEVICE, NULL,
        };
        int status = run_program(mkfs);
        if (status != 0) {
            boot_err_log("mke2fs", status);
            return status < 0 ? status : -EIO;
        }
        return 0;
    }
    rc = check_upper(info.state == UPPER_EXT4_BACKUP_ONLY);
    if (rc < 0)
        return rc;
    rc = probe_upper(&info);
    if (rc < 0)
        return rc;
    if (info.state != UPPER_EXT4)
        return -EUCLEAN;
    return info.has_journal ? 0 : add_journal();
}

static void remove_dir_contents(int dir_fd) {
    DIR *dir = fdopendir(dir_fd);
    if (!dir) {
        close(dir_fd);
        return;
    }
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        bool is_dir = (de->d_type == DT_DIR);
        if (de->d_type == DT_UNKNOWN) {
            struct stat st;
            if (fstatat(dirfd(dir), de->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISDIR(st.st_mode))
                is_dir = true;
        }
        if (is_dir) {
            int sub_fd = openat(dirfd(dir), de->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (sub_fd >= 0)
                remove_dir_contents(sub_fd);
            (void)unlinkat(dirfd(dir), de->d_name, AT_REMOVEDIR);
        } else {
            (void)unlinkat(dirfd(dir), de->d_name, 0);
        }
    }
    closedir(dir);
}

int storage_mount_root(void) {
    int rc = prepare_upper();
    if (rc < 0)
        return rc;
    if (mount(UPPER_DEVICE, UPPER_MOUNT, "ext4", MS_NOATIME, "discard") < 0 &&
        mount(UPPER_DEVICE, UPPER_MOUNT, "ext4", MS_NOATIME, NULL) < 0)
        return -errno;

    ensure_dir(UPPER_MOUNT "/root", 0755);
    int work_fd = open(UPPER_MOUNT "/work", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (work_fd >= 0) {
        remove_dir_contents(work_fd);
        (void)rmdir(UPPER_MOUNT "/work");
    } else if (errno == ENOTDIR) {
        (void)unlink(UPPER_MOUNT "/work");
    }
    if (mkdir(UPPER_MOUNT "/work", 0755) < 0 && errno != EEXIST)
        return -errno;

    if (mount("overlay", GUEST_ROOT, "overlay", 0,
              "lowerdir=/mnt/lower,upperdir=" UPPER_MOUNT "/root,workdir=" UPPER_MOUNT "/work") < 0)
        return -errno;
    if (mount(NULL, GUEST_ROOT, NULL, MS_SHARED, NULL) < 0)
        return -errno;
    g_daemon.root_fd = open(GUEST_ROOT, O_PATH | O_DIRECTORY | O_CLOEXEC);
    return g_daemon.root_fd < 0 ? -errno : 0;
}

int storage_mount_modules(void) {
    struct utsname uts;
    if (uname(&uts) < 0)
        return -errno;
    char shipped[PATH_MAX];
    char store[PATH_MAX];
    char upper[PATH_MAX];
    char work[PATH_MAX];
    if (snprintf(shipped, sizeof(shipped), "/lib/modules/%s", uts.release) >= (int)sizeof(shipped) ||
        snprintf(store, sizeof(store), UPPER_MOUNT "/modules/%s", uts.release) >= (int)sizeof(store) ||
        snprintf(upper, sizeof(upper), "%s/upper", store) >= (int)sizeof(upper) ||
        snprintf(work, sizeof(work), "%s/work", store) >= (int)sizeof(work))
        return -ENAMETOOLONG;

    ensure_dir("/lib", 0755);
    ensure_dir("/lib/modules", 0755);
    ensure_dir(shipped, 0755);
    ensure_dir(UPPER_MOUNT "/modules", 0755);
    ensure_dir(store, 0755);
    ensure_dir(upper, 0755);
    ensure_dir(work, 0755);

    int rc = fs_mkdir_p_jailed(shipped, 0755);
    if (rc < 0)
        return rc;
    int fd = fs_open_in_root(shipped + 1, O_PATH | O_DIRECTORY, 0);
    if (fd < 0)
        return fd;
    char target[32];
    char options[3 * PATH_MAX + 32];
    snprintf(target, sizeof(target), "/proc/self/fd/%d", fd);
    if (snprintf(options, sizeof(options), "lowerdir=%s,upperdir=%s,workdir=%s", shipped, upper, work) >=
        (int)sizeof(options)) {
        close(fd);
        return -ENAMETOOLONG;
    }
    rc = mount("overlay", target, "overlay", 0, options) < 0 ? -errno : 0;
    close(fd);
    return rc;
}

static int unmount_guest_children(void) {
    int result = 0;
    for (;;) {
        FILE *fp = setmntent("/proc/self/mounts", "re");
        if (!fp)
            return -errno;
        char selected[PATH_MAX] = "";
        size_t selected_len = 0;
        char buf[3 * PATH_MAX];
        struct mntent entry;
        while (getmntent_r(fp, &entry, buf, sizeof(buf))) {
            const char *dir = entry.mnt_dir;
            size_t len = strlen(dir);
            if (strncmp(dir, GUEST_ROOT "/", sizeof(GUEST_ROOT)) != 0 || len < selected_len ||
                len >= sizeof(selected))
                continue;
            memcpy(selected, dir, len + 1);
            selected_len = len;
        }
        endmntent(fp);
        if (selected_len == 0)
            return result;
        if (umount2(selected, 0) < 0) {
            int err = -errno;
            boot_err_log(selected, err);
            if (umount2(selected, MNT_DETACH) < 0)
                return err;
            result = err;
        }
    }
}

bool storage_teardown(void) {
    int children = unmount_guest_children();
    int overlay = umount2(GUEST_ROOT, 0) < 0 && errno != EINVAL ? -errno : 0;
    int upper_fd = open(UPPER_MOUNT, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    int flush = upper_fd < 0 ? -errno : (syncfs(upper_fd) < 0 ? -errno : 0);
    if (upper_fd >= 0)
        close(upper_fd);
    int upper = overlay == 0 ? (umount2(UPPER_MOUNT, 0) < 0 && errno != EINVAL ? -errno : 0) : -EBUSY;
    if (children < 0)
        boot_err_log("unmount guest submounts", children);
    if (overlay < 0)
        boot_err_log("unmount " GUEST_ROOT, overlay);
    if (flush < 0)
        boot_err_log("syncfs " UPPER_MOUNT, flush);
    if (upper < 0) {
        boot_err_log("unmount " UPPER_MOUNT, upper);
        (void)mount(NULL, UPPER_MOUNT, NULL, MS_REMOUNT | MS_RDONLY, NULL);
    }
    sync();
    return children == 0 && overlay == 0 && flush == 0 && upper == 0;
}
