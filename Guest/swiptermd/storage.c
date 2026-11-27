#include "swiptermd.h"

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
#include <sys/wait.h>
#include <unistd.h>

#define ROOT_DEVICE "/dev/vda"

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

typedef struct root_info {
    bool backup_only;
    bool has_journal;
    uint32_t block_size;
    uint64_t free_blocks;
} root_info_t;

static uint16_t get_le16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void decode_super(const uint8_t *sb, bool backup_only, root_info_t *info) {
    uint32_t log_block_size = get_le32(sb + 0x18);
    info->backup_only = backup_only;
    info->has_journal = (get_le32(sb + 0x5C) & EXT4_FEATURE_COMPAT_HAS_JOURNAL) != 0;
    info->block_size = log_block_size <= 6 ? 1024U << log_block_size : 0;
    info->free_blocks = get_le32(sb + 0x0C);
    if (get_le32(sb + 0x60) & EXT4_FEATURE_INCOMPAT_64BIT)
        info->free_blocks |= (uint64_t)get_le32(sb + 0x158) << 32;
}

static int probe(const char *device, root_info_t *info) {
    int fd = open(device, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    uint8_t block[EXT4_PROBE_SIZE];
    uint8_t backup[1024];
    int rc = 0;
    ssize_t n = pread(fd, block, sizeof(block), 0);
    if (n != (ssize_t)sizeof(block)) {
        rc = n < 0 ? -errno : -EIO;
    } else if (get_le16(block + EXT4_SUPER_OFFSET + 0x38) == EXT4_SUPER_MAGIC) {
        decode_super(block + EXT4_SUPER_OFFSET, false, info);
    } else if (pread(fd, backup, sizeof(backup), (off_t)EXT4_BACKUP_SUPER_BLOCK * EXT4_BACKUP_BLOCK_SIZE) ==
                   (ssize_t)sizeof(backup) &&
               get_le16(backup + 0x38) == EXT4_SUPER_MAGIC) {
        decode_super(backup, true, info);
    } else {
        rc = -EUCLEAN;
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

static int check(const char *device_path, bool from_backup) {
    char *device = (char *)device_path;
    char backup_block[16];
    snprintf(backup_block, sizeof(backup_block), "%d", EXT4_BACKUP_SUPER_BLOCK);
    char *const preen[] = {"/sbin/e2fsck", "-p", device, NULL};
    char *const preen_backup[] = {"/sbin/e2fsck", "-p", "-b", backup_block, device, NULL};
    int status = run_program(from_backup ? preen_backup : preen);
    if (status >= 0 && (status & E2FSCK_UNCORRECTED)) {
        boot_err_log("e2fsck -p", status);
        char *const repair[] = {"/sbin/e2fsck", "-f", "-y", device, NULL};
        char *const repair_backup[] = {"/sbin/e2fsck", "-f", "-y", "-b", backup_block, device, NULL};
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
        root_info_t info;
        int rc = probe(ROOT_DEVICE, &info);
        if (rc < 0)
            return rc;
        if (info.backup_only || info.block_size == 0)
            return -EUCLEAN;
        if (info.has_journal)
            return 0;
        uint64_t journal_blocks = (uint64_t)journal_sizes_mib[i] * 1024 * 1024 / info.block_size;
        if (journal_blocks + JOURNAL_RESERVE_BLOCKS > info.free_blocks)
            continue;
        char size[32];
        snprintf(size, sizeof(size), "size=%u", journal_sizes_mib[i]);
        char *const tune[] = {"/sbin/tune2fs", "-j", "-J", size, ROOT_DEVICE, NULL};
        int status = run_program(tune);
        if (status == 0) {
            if (i > 0)
                console_log("swiptermd: added a %u MiB journal to the root filesystem\n", journal_sizes_mib[i]);
            return 0;
        }
        boot_err_log("tune2fs -j", status);
        rc = check(ROOT_DEVICE, false);
        if (rc < 0)
            return rc;
    }
    console_log("swiptermd: the root filesystem has too little free space for a journal\n");
    return -ENOSPC;
}

int storage_check(const char *device) {
    root_info_t info;
    int rc = probe(device, &info);
    if (rc < 0)
        return rc;
    rc = check(device, info.backup_only);
    if (rc < 0)
        return rc;
    rc = probe(device, &info);
    if (rc < 0)
        return rc;
    return info.backup_only ? -EUCLEAN : 0;
}

static int prepare_root(void) {
    int rc = storage_check(ROOT_DEVICE);
    if (rc < 0)
        return rc;
    root_info_t info;
    rc = probe(ROOT_DEVICE, &info);
    if (rc < 0)
        return rc;
    return info.has_journal ? 0 : add_journal();
}

int storage_mount_root(void) {
    int rc = prepare_root();
    if (rc < 0)
        return rc;
    if (mount(ROOT_DEVICE, GUEST_ROOT, "ext4", MS_NOATIME, "discard") < 0 &&
        mount(ROOT_DEVICE, GUEST_ROOT, "ext4", MS_NOATIME, NULL) < 0)
        return -errno;
    if (mount(NULL, GUEST_ROOT, NULL, MS_SHARED, NULL) < 0)
        return -errno;
    g_daemon.root_fd = open(GUEST_ROOT, O_PATH | O_DIRECTORY | O_CLOEXEC);
    return g_daemon.root_fd < 0 ? -errno : 0;
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

void storage_teardown(void) {
    int children = unmount_guest_children();
    int root_fd = open(GUEST_ROOT, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    int flush = root_fd < 0 ? -errno : (syncfs(root_fd) < 0 ? -errno : 0);
    if (root_fd >= 0)
        close(root_fd);
    int root = umount2(GUEST_ROOT, 0) < 0 && errno != EINVAL ? -errno : 0;
    if (children < 0)
        boot_err_log("unmount guest submounts", children);
    if (flush < 0)
        boot_err_log("syncfs " GUEST_ROOT, flush);
    if (root < 0) {
        boot_err_log("unmount " GUEST_ROOT, root);
        (void)mount(NULL, GUEST_ROOT, NULL, MS_REMOUNT | MS_RDONLY, NULL);
    }
    sync();
}
