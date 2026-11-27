#include "swiptermd.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/openat2.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define GUEST_DISKS "/run/disks"
#define OLD_GUEST_ROOT "mnt/root"
#define POLICY_RC_D "usr/sbin/policy-rc.d"

enum {
    RELEASE_TIMEOUT_MS = 10000,
    RELEASE_POLL_MS = 10,
    SANDBOX_STACK_SIZE = 64 * 1024,
};

typedef struct disk {
    uint32_t id;
    unsigned mmio_addr;
    bool read_only;
    bool mounted;
    bool busy;
    unsigned sandboxes;
    char node[32];
    char base[32];
    char **binds;
    size_t bind_count;
    struct disk *next;
} disk_t;

typedef struct disk_job_result {
    int32_t rc;
    int32_t mounted;
} disk_job_result_t;

typedef struct disk_job {
    disk_t *disk;
    uint16_t opcode;
    uint32_t req_id;
    int result_fd;
    ev_node_t ev;
} disk_job_t;

struct disk_sandbox {
    disk_t *disk;
    pid_t supervisor;
    bool policy_placeholder;
    struct disk_sandbox *next;
};

typedef struct sandbox_args {
    const disk_t *disk;
    const char *hostname;
    int ready_wr;
} sandbox_args_t;

static disk_t *g_disks;
static disk_sandbox_t *g_sandboxes;

static disk_t *disk_find(uint32_t id) {
    for (disk_t *d = g_disks; d; d = d->next) {
        if (d->id == id)
            return d;
    }
    return NULL;
}

static void disk_remove(disk_t *disk) {
    for (disk_t **pp = &g_disks; *pp; pp = &(*pp)->next) {
        if (*pp == disk) {
            *pp = disk->next;
            break;
        }
    }
    virtio_mmio_unbind(disk->mmio_addr);
    (void)rmdir(disk->base);
    for (size_t i = 0; i < disk->bind_count; i++)
        free(disk->binds[i]);
    free(disk->binds);
    free(disk);
}

static int first_entry(const char *directory, const char *prefix, char *name, size_t cap) {
    DIR *dir = opendir(directory);
    if (!dir)
        return -errno;
    int rc = -ENODEV;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' || strncmp(entry->d_name, prefix, strlen(prefix)) != 0)
            continue;
        rc = snprintf(name, cap, "%s", entry->d_name) < (int)cap ? 0 : -ENAMETOOLONG;
        break;
    }
    closedir(dir);
    return rc;
}

static int block_node(unsigned mmio_addr, char *node, size_t cap) {
    char path[PATH_MAX];
    char name[NAME_MAX + 1];
    snprintf(path, sizeof(path), "/sys/bus/platform/devices/%x.virtio_mmio", mmio_addr);
    int rc = first_entry(path, "virtio", name, sizeof(name));
    if (rc < 0)
        return rc;
    size_t len = strlen(path);
    snprintf(path + len, sizeof(path) - len, "/%s/block", name);
    rc = first_entry(path, "", name, sizeof(name));
    if (rc < 0)
        return rc;
    return snprintf(node, cap, "/dev/%s", name) < (int)cap ? 0 : -ENAMETOOLONG;
}

static int release_device(const char *node) {
    for (int waited = 0;; waited += RELEASE_POLL_MS) {
        int fd = open(node, O_RDWR | O_EXCL | O_CLOEXEC);
        if (fd >= 0) {
            (void)fsync(fd);
            (void)ioctl(fd, BLKFLSBUF);
            close(fd);
            return 0;
        }
        if (errno != EBUSY)
            return -errno;
        if (waited >= RELEASE_TIMEOUT_MS)
            return -EBUSY;
        usleep(RELEASE_POLL_MS * 1000);
    }
}

static disk_job_result_t attach_run(const disk_t *disk) {
    disk_job_result_t result = {.rc = storage_check(disk->node)};
    if (result.rc < 0)
        return result;
    unsigned long flags = MS_NOATIME | (disk->read_only ? MS_RDONLY : 0);
    if ((disk->read_only || mount(disk->node, disk->base, "ext4", flags, "discard") < 0) &&
        mount(disk->node, disk->base, "ext4", flags, NULL) < 0)
        result.rc = -errno;
    else if (mount(NULL, disk->base, NULL, MS_PRIVATE, NULL) < 0)
        result.rc = -errno;
    result.mounted = result.rc == 0;
    return result;
}

static disk_job_result_t detach_run(const disk_t *disk) {
    disk_job_result_t result = {.mounted = disk->mounted};
    if (disk->mounted) {
        if (umount2(disk->base, 0) < 0) {
            result.rc = -errno;
            return result;
        }
        result.mounted = false;
    }
    result.rc = release_device(disk->node);
    return result;
}

static void on_job_done(ev_node_t *node, uint32_t events) {
    (void)events;
    disk_job_t *job = node->ctx;
    disk_t *disk = job->disk;
    disk_job_result_t result = {.rc = -EIO, .mounted = disk->mounted};
    (void)read_all_blocking(job->result_fd, &result, sizeof(result));
    ev_del(&job->ev);
    close(job->result_fd);

    disk->busy = false;
    disk->mounted = result.mounted != 0;
    bool attached = job->opcode == SWIPTERMD_OP_DISK_ATTACH ? result.rc == 0 : result.rc != 0;
    if (!attached)
        disk_remove(disk);
    ctrl_send_err_reply(job->opcode, job->req_id, result.rc);
    free(job);
}

static int job_start(disk_t *disk, uint16_t opcode, uint32_t req_id, disk_job_result_t (*run)(const disk_t *)) {
    disk_job_t *job = calloc(1, sizeof(*job));
    if (!job)
        return -ENOMEM;
    int result_pipe[2];
    if (pipe2(result_pipe, O_CLOEXEC) < 0) {
        int err = -errno;
        free(job);
        return err;
    }
    pid_t pid = fork();
    if (pid < 0) {
        int err = -errno;
        close(result_pipe[0]);
        close(result_pipe[1]);
        free(job);
        return err;
    }
    if (pid == 0) {
        close(result_pipe[0]);
        disk_job_result_t result = run(disk);
        (void)write_all_blocking(result_pipe[1], &result, sizeof(result));
        _exit(0);
    }
    close(result_pipe[1]);
    job->disk = disk;
    job->opcode = opcode;
    job->req_id = req_id;
    job->result_fd = result_pipe[0];
    disk->busy = true;
    ev_add(&job->ev, job->result_fd, EPOLLIN, on_job_done, job);
    return 0;
}

void disk_handle_attach(uint32_t req_id, const uint8_t *body, uint32_t len) {
    uint32_t id;
    uint32_t slot;
    bool read_only;
    if (!swiptermd_dec_disk_attach_req(body, len, &id, &slot, &read_only) || slot < VIRTIO_SLOT_DISK_BASE ||
        slot >= VIRTIO_SLOT_DISK_BASE + VIRTIO_SLOT_DISK_COUNT || disk_find(id)) {
        ctrl_send_err_reply(SWIPTERMD_OP_DISK_ATTACH, req_id, -EINVAL);
        return;
    }
    disk_t *disk = calloc(1, sizeof(*disk));
    if (!disk) {
        ctrl_send_err_reply(SWIPTERMD_OP_DISK_ATTACH, req_id, -ENOMEM);
        return;
    }
    disk->id = id;
    disk->read_only = read_only;
    disk->mmio_addr = VIRTIO_MMIO_ADDR(slot);
    snprintf(disk->base, sizeof(disk->base), GUEST_DISKS "/%u", id);
    disk->next = g_disks;
    g_disks = disk;

    int rc = virtio_mmio_bind(disk->mmio_addr);
    if (rc >= 0)
        rc = block_node(disk->mmio_addr, disk->node, sizeof(disk->node));
    if (rc == 0 && ((mkdir(GUEST_DISKS, 0755) < 0 && errno != EEXIST) || mkdir(disk->base, 0755) < 0))
        rc = -errno;
    if (rc == 0)
        rc = job_start(disk, SWIPTERMD_OP_DISK_ATTACH, req_id, attach_run);
    if (rc < 0) {
        disk_remove(disk);
        ctrl_send_err_reply(SWIPTERMD_OP_DISK_ATTACH, req_id, rc);
    }
}

static int unmount_bind(disk_t *disk, size_t index) {
    char full[PATH_MAX];
    if (snprintf(full, sizeof(full), GUEST_ROOT "%s", disk->binds[index]) >= (int)sizeof(full))
        return -ENAMETOOLONG;
    if (umount2(full, 0) < 0 && errno != EINVAL && errno != ENOENT)
        return -errno;
    free(disk->binds[index]);
    disk->binds[index] = disk->binds[--disk->bind_count];
    return 0;
}

void disk_handle_detach(uint32_t req_id, const uint8_t *body, uint32_t len) {
    uint32_t id;
    disk_t *disk = swiptermd_dec_u32(body, len, &id) ? disk_find(id) : NULL;
    int rc = !disk ? -ENODEV : disk->busy || disk->sandboxes > 0 ? -EBUSY : 0;
    while (rc == 0 && disk->bind_count > 0)
        rc = unmount_bind(disk, disk->bind_count - 1);
    if (rc == 0)
        rc = job_start(disk, SWIPTERMD_OP_DISK_DETACH, req_id, detach_run);
    if (rc < 0)
        ctrl_send_err_reply(SWIPTERMD_OP_DISK_DETACH, req_id, rc);
}

void disk_handle_mount(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = {.data = body, .len = len};
    disk_t *disk = disk_find(swiptermd_r_u32(&r));
    char guest_path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, guest_path, sizeof(guest_path))) {
        ctrl_send_err_reply(SWIPTERMD_OP_DISK_MOUNT, req_id, -EINVAL);
        return;
    }
    if (!disk || !disk->mounted || disk->busy) {
        ctrl_send_err_reply(SWIPTERMD_OP_DISK_MOUNT, req_id, disk && disk->busy ? -EBUSY : -ENODEV);
        return;
    }
    char norm[PATH_MAX];
    int fd = fs_prepare_mountpoint(guest_path, norm);
    if (fd < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_DISK_MOUNT, req_id, fd);
        return;
    }
    char target[32];
    snprintf(target, sizeof(target), "/proc/self/fd/%d", fd);
    char *path = strdup(norm);
    char **grown = path ? realloc(disk->binds, (disk->bind_count + 1) * sizeof(*disk->binds)) : NULL;
    int rc = !grown ? -ENOMEM : mount(disk->base, target, NULL, MS_BIND, NULL) < 0 ? -errno : 0;
    close(fd);
    if (grown)
        disk->binds = grown;
    if (rc == 0)
        disk->binds[disk->bind_count++] = path;
    else
        free(path);
    ctrl_send_err_reply(SWIPTERMD_OP_DISK_MOUNT, req_id, rc);
}

void disk_handle_unmount(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = {.data = body, .len = len};
    disk_t *disk = disk_find(swiptermd_r_u32(&r));
    char guest_path[PATH_MAX];
    char norm[PATH_MAX];
    int rc = !swiptermd_r_str_copy(&r, guest_path, sizeof(guest_path)) ? -EINVAL
             : !disk                                                   ? -ENODEV
                                                                       : fs_normalize_path(guest_path, norm, sizeof(norm));
    if (rc == 0) {
        rc = -EINVAL;
        for (size_t i = 0; i < disk->bind_count; i++) {
            if (strcmp(disk->binds[i], norm) == 0) {
                rc = unmount_bind(disk, i);
                break;
            }
        }
    }
    ctrl_send_err_reply(SWIPTERMD_OP_DISK_UNMOUNT, req_id, rc);
}

static int sandbox_mount_api(void) {
    static const struct {
        const char *path;
        mode_t mode;
    } mountpoints[] = {{"/proc", 0555}, {"/sys", 0555}, {"/dev", 0755}, {"/run", 0755}, {"/tmp", 01777}};
    for (size_t i = 0; i < sizeof(mountpoints) / sizeof(mountpoints[0]); i++) {
        if (mkdir(mountpoints[i].path, mountpoints[i].mode) < 0 && errno != EEXIST && errno != EROFS)
            return -errno;
    }
    const unsigned long api_flags = MS_NOSUID | MS_NODEV | MS_NOEXEC;
    if (mount("proc", "/proc", "proc", api_flags, NULL) < 0 ||
        mount("sysfs", "/sys", "sysfs", api_flags | MS_RDONLY, NULL) < 0 ||
        mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755,size=4m") < 0 ||
        mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755") < 0 ||
        mount("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777") < 0)
        return -errno;
    int rc = dev_populate("/dev");
    if (rc < 0)
        return rc;
    if (mkdir("/dev/pts", 0755) < 0 || mkdir("/dev/shm", 01777) < 0 || mkdir("/run/host", 0755) < 0 ||
        mount("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777") < 0)
        return -errno;
    return write_file(AT_FDCWD, "/run/host/resolv.conf", GUEST_DEFAULT_RESOLV_CONF, O_CREAT | O_EXCL, 0644);
}

static void sandbox_resolv_conf(void) {
    static const char path[] = "/etc/resolv.conf";
    struct stat st;
    if (stat(path, &st) == 0) {
        (void)mount("/run/host/resolv.conf", path, NULL, MS_BIND, NULL);
        return;
    }
    char target[PATH_MAX] = "/etc/";
    ssize_t n = readlink(path, target + 5, sizeof(target) - 6);
    if (n <= 0) {
        (void)write_file(AT_FDCWD, path, GUEST_DEFAULT_RESOLV_CONF, O_CREAT | O_EXCL, 0644);
        return;
    }
    target[5 + n] = '\0';
    char *resolved = target[5] == '/' ? target + 5 : target;
    for (char *slash = strchr(resolved + 1, '/'); slash; slash = strchr(slash + 1, '/')) {
        *slash = '\0';
        (void)mkdir(resolved, 0755);
        *slash = '/';
    }
    (void)write_file(AT_FDCWD, resolved, GUEST_DEFAULT_RESOLV_CONF, O_CREAT | O_EXCL, 0644);
}

static bool sandbox_deny_services(void) {
    struct stat st;
    if (stat("/var/lib/dpkg", &st) < 0)
        return false;
    bool placeholder = false;
    if (lstat("/" POLICY_RC_D, &st) < 0) {
        int fd = open("/" POLICY_RC_D, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
        if (fd < 0)
            return false;
        close(fd);
        placeholder = true;
    }
    if (write_file(AT_FDCWD, "/run/host/policy-rc.d", "#!/bin/sh\nexit 101\n", O_CREAT | O_EXCL, 0755) == 0)
        (void)mount("/run/host/policy-rc.d", "/" POLICY_RC_D, NULL, MS_BIND, NULL);
    return placeholder;
}

static int sandbox_assemble(const sandbox_args_t *args) {
    if (args->hostname[0] != '\0' && sethostname(args->hostname, strlen(args->hostname)) < 0)
        return -errno;
    int rc = container_enter(true);
    if (rc < 0)
        return rc;
    int old_root = open("/", O_PATH | O_DIRECTORY | O_CLOEXEC);
    if (old_root < 0 || mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0 || chdir(args->disk->base) < 0 ||
        mount(".", "/", NULL, MS_MOVE, NULL) < 0 || chroot(".") < 0 || chdir("/") < 0)
        return -errno;
    rc = sandbox_mount_api();
    if (rc < 0)
        return rc;
    if (fchdir(old_root) < 0 || mount(OLD_GUEST_ROOT "/dev/pts", "/dev/pts", NULL, MS_BIND, NULL) < 0)
        return -errno;
    static const char *const stale[] = {OLD_GUEST_ROOT, "run", "dev", "sys", "proc"};
    for (size_t i = 0; i < sizeof(stale) / sizeof(stale[0]); i++) {
        if (umount2(stale[i], MNT_DETACH) < 0)
            return -errno;
    }
    if (chdir("/") < 0)
        return -errno;
    close(old_root);
    sandbox_resolv_conf();
    return sandbox_deny_services() ? 1 : 0;
}

static int sandbox_entry(void *arg) {
    const sandbox_args_t *args = arg;
    int32_t result = sandbox_assemble(args);
    (void)write_all_blocking(args->ready_wr, &result, sizeof(result));
    if (result < 0)
        _exit(127);
    close_other_fds(NULL, 0);
    redirect_stdio(open("/dev/null", O_RDWR));
    sigset_t all;
    sigfillset(&all);
    sigprocmask(SIG_SETMASK, &all, NULL);
    for (;;) {
        (void)sigwaitinfo(&all, NULL);
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
    }
}

static void sandbox_kill(disk_sandbox_t *sandbox) {
    kill(sandbox->supervisor, SIGKILL);
    int status;
    while (waitpid(sandbox->supervisor, &status, 0) < 0 && errno == EINTR) {}
    if (sandbox->policy_placeholder) {
        struct open_how how = {.flags = O_PATH | O_NOFOLLOW | O_CLOEXEC, .resolve = RESOLVE_IN_ROOT};
        int root = open(sandbox->disk->base, O_PATH | O_DIRECTORY | O_CLOEXEC);
        long file = root < 0 ? -1 : syscall(SYS_openat2, root, POLICY_RC_D, &how, sizeof(how));
        struct stat st;
        if (file >= 0 && fstat((int)file, &st) == 0 && S_ISREG(st.st_mode) && st.st_size == 0) {
            how.flags = O_PATH | O_DIRECTORY | O_CLOEXEC;
            long directory = syscall(SYS_openat2, root, "usr/sbin", &how, sizeof(how));
            if (directory >= 0) {
                (void)unlinkat((int)directory, "policy-rc.d", 0);
                close((int)directory);
            }
        }
        if (file >= 0)
            close((int)file);
        if (root >= 0)
            close(root);
    }
    sandbox->disk->sandboxes--;
}

int disk_sandbox_open(uint32_t disk_id, const char *hostname, launch_target_t *target, disk_sandbox_t **sandbox_out) {
    disk_t *disk = disk_find(disk_id);
    if (!disk || !disk->mounted)
        return -ENODEV;
    if (disk->busy)
        return -EBUSY;
    disk_sandbox_t *sandbox = calloc(1, sizeof(*sandbox));
    uint8_t *stack = malloc(SANDBOX_STACK_SIZE);
    int ready_pipe[2] = {-1, -1};
    int rc = !sandbox || !stack ? -ENOMEM : pipe2(ready_pipe, O_CLOEXEC) < 0 ? -errno : 0;
    pid_t pid = -1;
    if (rc == 0) {
        sandbox_args_t args = {.disk = disk, .hostname = hostname, .ready_wr = ready_pipe[1]};
        pid = clone(sandbox_entry, stack + SANDBOX_STACK_SIZE,
                    CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | SIGCHLD, &args);
        if (pid < 0)
            rc = -errno;
    }
    if (ready_pipe[1] >= 0)
        close(ready_pipe[1]);
    free(stack);
    if (rc < 0) {
        if (ready_pipe[0] >= 0)
            close(ready_pipe[0]);
        free(sandbox);
        return rc;
    }

    int32_t result = -EIO;
    (void)read_all_blocking(ready_pipe[0], &result, sizeof(result));
    close(ready_pipe[0]);
    sandbox->disk = disk;
    sandbox->supervisor = pid;
    sandbox->policy_placeholder = result > 0;
    disk->sandboxes++;
    *target = (launch_target_t){
        .net_ns_fd = dup(network_guest_fd()),
        .ipc_ns_fd = namespace_open(pid, "ipc"),
        .uts_ns_fd = namespace_open(pid, "uts"),
        .mnt_ns_fd = namespace_open(pid, "mnt"),
        .pid_ns_fd = namespace_open(pid, "pid"),
        .cgroup_ns_fd = namespace_open(pid, "cgroup"),
        .procs_fd = disk_procs_fd(),
    };
    if (result < 0 || target->net_ns_fd < 0 || target->ipc_ns_fd < 0 || target->uts_ns_fd < 0 ||
        target->mnt_ns_fd < 0 || target->pid_ns_fd < 0 || target->cgroup_ns_fd < 0 || target->procs_fd < 0) {
        launch_target_close(target);
        sandbox_kill(sandbox);
        free(sandbox);
        return result < 0 ? result : -ESRCH;
    }
    sandbox->next = g_sandboxes;
    g_sandboxes = sandbox;
    *sandbox_out = sandbox;
    return 0;
}

void disk_sandbox_close(disk_sandbox_t *sandbox) {
    for (disk_sandbox_t **pp = &g_sandboxes; *pp; pp = &(*pp)->next) {
        if (*pp == sandbox) {
            *pp = sandbox->next;
            break;
        }
    }
    sandbox_kill(sandbox);
    free(sandbox);
}

void disk_shutdown(void) {
    while (g_sandboxes)
        disk_sandbox_close(g_sandboxes);
    for (disk_t *disk = g_disks; disk; disk = disk->next) {
        for (size_t i = 0; i < disk->bind_count; i++) {
            char full[PATH_MAX];
            if (snprintf(full, sizeof(full), GUEST_ROOT "%s", disk->binds[i]) < (int)sizeof(full) &&
                umount2(full, 0) < 0)
                (void)umount2(full, MNT_DETACH);
        }
        if (disk->mounted && umount2(disk->base, 0) < 0) {
            boot_err_log(disk->base, -errno);
            (void)umount2(disk->base, MNT_DETACH);
        }
        int rc = release_device(disk->node);
        if (rc < 0)
            boot_err_log(disk->node, rc);
    }
}
