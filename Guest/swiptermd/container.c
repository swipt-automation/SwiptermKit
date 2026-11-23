#include "swiptermd.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/bpf.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#define CGROUP_ROOT "/sys/fs/cgroup"
#define CGROUP_HOST CGROUP_ROOT "/host"
#define CGROUP_MACHINE CGROUP_ROOT "/machine"
#define CGROUP_PLAIN_SESSIONS CGROUP_MACHINE "/swipterm-sessions"
#define SESSIONS_UNIT "swipterm-sessions.service"
#define SYSTEMD_CONTROL GUEST_ROOT "/run/systemd/system.control"

static const unsigned long api_mount_flags = MS_NOSUID | MS_NODEV | MS_NOEXEC;

static int g_device_map = -1;
static int g_sessions_dir = -1;

typedef struct device_key {
    uint32_t type;
    uint32_t major_number;
    uint32_t minor_number;
} device_key_t;

enum {
    DEVICE_ANY_MINOR = UINT32_MAX,
    PTS_MAJOR_FIRST = 136,
    PTS_MAJOR_LAST = 143,
    LOOP_MAJOR = 7,
    LOOP_NODE_COUNT = 8,
};

static const device_key_t default_devices[] = {
    {BPF_DEVCG_DEV_CHAR, 1, 3}, {BPF_DEVCG_DEV_CHAR, 1, 5}, {BPF_DEVCG_DEV_CHAR, 1, 7},
    {BPF_DEVCG_DEV_CHAR, 1, 8}, {BPF_DEVCG_DEV_CHAR, 1, 9},
    {BPF_DEVCG_DEV_CHAR, 5, 0}, {BPF_DEVCG_DEV_CHAR, 5, 1}, {BPF_DEVCG_DEV_CHAR, 5, 2},
    {BPF_DEVCG_DEV_CHAR, 10, 200}, {BPF_DEVCG_DEV_CHAR, 10, 229}, {BPF_DEVCG_DEV_CHAR, 10, 203},
    {BPF_DEVCG_DEV_CHAR, 10, 235}, {BPF_DEVCG_DEV_CHAR, 10, 237},
    {BPF_DEVCG_DEV_CHAR, 0, 0},
    {BPF_DEVCG_DEV_BLOCK, LOOP_MAJOR, DEVICE_ANY_MINOR},
};

static long bpf_call(enum bpf_cmd cmd, union bpf_attr *attr) {
    return syscall(SYS_bpf, cmd, attr, sizeof(*attr));
}

static int device_allow(const device_key_t *key) {
    uint32_t allowed = 1;
    union bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.map_fd = (uint32_t)g_device_map;
    attr.key = (uint64_t)(uintptr_t)key;
    attr.value = (uint64_t)(uintptr_t)&allowed;
    attr.flags = BPF_ANY;
    return bpf_call(BPF_MAP_UPDATE_ELEM, &attr) < 0 ? -errno : 0;
}

int device_permit(unsigned major_number, unsigned minor_number) {
    device_key_t key = {BPF_DEVCG_DEV_CHAR, major_number, minor_number};
    return device_allow(&key);
}

void device_revoke(unsigned major_number, unsigned minor_number) {
    device_key_t key = {BPF_DEVCG_DEV_CHAR, major_number, minor_number};
    union bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.map_fd = (uint32_t)g_device_map;
    attr.key = (uint64_t)(uintptr_t)&key;
    (void)bpf_call(BPF_MAP_DELETE_ELEM, &attr);
}

#define BPF_INSN(code_, dst_, src_, off_, imm_) \
    ((struct bpf_insn){.code = (code_), .dst_reg = (dst_), .src_reg = (src_), .off = (off_), .imm = (imm_)})
#define BPF_LOAD_CTX(dst, offset) BPF_INSN(BPF_LDX | BPF_MEM | BPF_W, dst, BPF_REG_1, offset, 0)
#define BPF_RETURN(value) BPF_INSN(BPF_ALU64 | BPF_MOV | BPF_K, BPF_REG_0, 0, 0, value), \
    BPF_INSN(BPF_JMP | BPF_EXIT, 0, 0, 0, 0)
#define BPF_LOOKUP_KEY(map_fd)                                                     \
    BPF_INSN(BPF_LD | BPF_DW | BPF_IMM, BPF_REG_1, BPF_PSEUDO_MAP_FD, 0, map_fd), \
    BPF_INSN(0, 0, 0, 0, 0),                                                       \
    BPF_INSN(BPF_ALU64 | BPF_MOV | BPF_X, BPF_REG_2, BPF_REG_10, 0, 0),           \
    BPF_INSN(BPF_ALU64 | BPF_ADD | BPF_K, BPF_REG_2, 0, 0, -12),                  \
    BPF_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem)

static int device_policy_attach(void) {
    union bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.map_type = BPF_MAP_TYPE_HASH;
    attr.key_size = sizeof(device_key_t);
    attr.value_size = sizeof(uint32_t);
    attr.max_entries = 1024;
    g_device_map = (int)bpf_call(BPF_MAP_CREATE, &attr);
    if (g_device_map < 0)
        return -errno;
    (void)fcntl(g_device_map, F_SETFD, FD_CLOEXEC);
    for (size_t i = 0; i < sizeof(default_devices) / sizeof(default_devices[0]); i++) {
        int rc = device_allow(&default_devices[i]);
        if (rc < 0)
            return rc;
    }
    for (unsigned major_number = PTS_MAJOR_FIRST; major_number <= PTS_MAJOR_LAST; major_number++) {
        int rc = device_permit(major_number, DEVICE_ANY_MINOR);
        if (rc < 0)
            return rc;
    }

    const struct bpf_insn program[] = {
        BPF_LOAD_CTX(BPF_REG_2, 0),
        BPF_INSN(BPF_ALU | BPF_AND | BPF_K, BPF_REG_2, 0, 0, 0xFFFF),
        BPF_LOAD_CTX(BPF_REG_3, 4),
        BPF_LOAD_CTX(BPF_REG_4, 8),
        BPF_INSN(BPF_STX | BPF_MEM | BPF_W, BPF_REG_10, BPF_REG_2, -12, 0),
        BPF_INSN(BPF_STX | BPF_MEM | BPF_W, BPF_REG_10, BPF_REG_3, -8, 0),
        BPF_INSN(BPF_STX | BPF_MEM | BPF_W, BPF_REG_10, BPF_REG_4, -4, 0),
        BPF_LOOKUP_KEY(g_device_map),
        BPF_INSN(BPF_JMP | BPF_JEQ | BPF_K, BPF_REG_0, 0, 2, 0),
        BPF_RETURN(1),
        BPF_INSN(BPF_ST | BPF_MEM | BPF_W, BPF_REG_10, 0, -4, (int32_t)DEVICE_ANY_MINOR),
        BPF_LOOKUP_KEY(g_device_map),
        BPF_INSN(BPF_JMP | BPF_JEQ | BPF_K, BPF_REG_0, 0, 2, 0),
        BPF_RETURN(1),
        BPF_RETURN(0),
    };
    static const char license[] = "GPL";
    memset(&attr, 0, sizeof(attr));
    attr.prog_type = BPF_PROG_TYPE_CGROUP_DEVICE;
    attr.expected_attach_type = BPF_CGROUP_DEVICE;
    attr.insns = (uint64_t)(uintptr_t)program;
    attr.insn_cnt = sizeof(program) / sizeof(program[0]);
    attr.license = (uint64_t)(uintptr_t)license;
    int prog = (int)bpf_call(BPF_PROG_LOAD, &attr);
    if (prog < 0)
        return -errno;
    int cgroup = open(CGROUP_MACHINE, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (cgroup < 0) {
        int err = -errno;
        close(prog);
        return err;
    }
    memset(&attr, 0, sizeof(attr));
    attr.target_fd = (uint32_t)cgroup;
    attr.attach_bpf_fd = (uint32_t)prog;
    attr.attach_type = BPF_CGROUP_DEVICE;
    attr.attach_flags = BPF_F_ALLOW_MULTI;
    int rc = bpf_call(BPF_PROG_ATTACH, &attr) < 0 ? -errno : 0;
    close(cgroup);
    close(prog);
    return rc;
}

static void enable_root_controllers(void) {
    int fd = open(CGROUP_ROOT "/cgroup.controllers", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return;
    char controllers[512];
    ssize_t n = read(fd, controllers, sizeof(controllers) - 1);
    close(fd);
    if (n <= 0)
        return;
    controllers[n] = '\0';
    char *save = NULL;
    for (char *name = strtok_r(controllers, " \n", &save); name; name = strtok_r(NULL, " \n", &save)) {
        char request[64];
        if (snprintf(request, sizeof(request), "+%s", name) < (int)sizeof(request))
            (void)write_file(AT_FDCWD, CGROUP_ROOT "/cgroup.subtree_control", request, 0, 0);
    }
}

int cgroup_setup(void) {
    if (mount("cgroup2", CGROUP_ROOT, "cgroup2", api_mount_flags, "nsdelegate") < 0)
        return -errno;
    if ((mkdir(CGROUP_HOST, 0755) < 0 && errno != EEXIST) || (mkdir(CGROUP_MACHINE, 0755) < 0 && errno != EEXIST))
        return -errno;
    int rc = write_file(AT_FDCWD, CGROUP_HOST "/cgroup.procs", "0", 0, 0);
    if (rc < 0)
        return rc;
    enable_root_controllers();
    return device_policy_attach();
}

static int make_mountpoint(const char *path, mode_t mode) {
    return mkdir(path, mode) < 0 && errno != EEXIST ? -errno : 0;
}

static int make_device(const char *path, mode_t mode, unsigned major_number, unsigned minor_number) {
    return mknod(path, mode, makedev(major_number, minor_number)) < 0 ? -errno : 0;
}

static int assemble_dev(void) {
    static const struct {
        const char *path;
        mode_t mode;
        unsigned major_number;
        unsigned minor_number;
    } nodes[] = {
        {GUEST_ROOT "/dev/null", S_IFCHR | 0666, 1, 3},
        {GUEST_ROOT "/dev/zero", S_IFCHR | 0666, 1, 5},
        {GUEST_ROOT "/dev/full", S_IFCHR | 0666, 1, 7},
        {GUEST_ROOT "/dev/random", S_IFCHR | 0666, 1, 8},
        {GUEST_ROOT "/dev/urandom", S_IFCHR | 0666, 1, 9},
        {GUEST_ROOT "/dev/tty", S_IFCHR | 0666, 5, 0},
        {GUEST_ROOT "/dev/console", S_IFCHR | 0600, 5, 1},
        {GUEST_ROOT "/dev/fuse", S_IFCHR | 0666, 10, 229},
        {GUEST_ROOT "/dev/cuse", S_IFCHR | 0600, 10, 203},
        {GUEST_ROOT "/dev/autofs", S_IFCHR | 0600, 10, 235},
        {GUEST_ROOT "/dev/loop-control", S_IFCHR | 0660, 10, 237},
        {GUEST_ROOT "/dev/net/tun", S_IFCHR | 0666, 10, 200},
    };
    static const struct {
        const char *target;
        const char *path;
    } links[] = {
        {"pts/ptmx", GUEST_ROOT "/dev/ptmx"},
        {"/proc/self/fd", GUEST_ROOT "/dev/fd"},
        {"/proc/self/fd/0", GUEST_ROOT "/dev/stdin"},
        {"/proc/self/fd/1", GUEST_ROOT "/dev/stdout"},
        {"/proc/self/fd/2", GUEST_ROOT "/dev/stderr"},
    };
    if (mount("tmpfs", GUEST_ROOT "/dev", "tmpfs", MS_NOSUID, "mode=0755,size=4m") < 0)
        return -errno;
    static const char *const dirs[] = {
        GUEST_ROOT "/dev/pts", GUEST_ROOT "/dev/shm", GUEST_ROOT "/dev/net", GUEST_ROOT "/dev/mqueue",
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        if (mkdir(dirs[i], i == 1 ? 01777 : 0755) < 0)
            return -errno;
    }
    for (size_t i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++) {
        int rc = make_device(nodes[i].path, nodes[i].mode, nodes[i].major_number, nodes[i].minor_number);
        if (rc < 0)
            return rc;
    }
    for (unsigned i = 0; i < LOOP_NODE_COUNT; i++) {
        char path[64];
        snprintf(path, sizeof(path), GUEST_ROOT "/dev/loop%u", i);
        int rc = make_device(path, S_IFBLK | 0660, LOOP_MAJOR, i);
        if (rc < 0)
            return rc;
    }
    for (size_t i = 0; i < sizeof(links) / sizeof(links[0]); i++) {
        if (symlink(links[i].target, links[i].path) < 0)
            return -errno;
    }
    if (mount("devpts", GUEST_ROOT "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC,
              "newinstance,ptmxmode=0666,mode=0620,gid=5") < 0 ||
        mount("tmpfs", GUEST_ROOT "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777") < 0)
        return -errno;
    return 0;
}

static int assemble_sys(void) {
    int rc = network_enter_guest();
    if (rc < 0)
        return rc;
    rc = mount("sysfs", GUEST_ROOT "/sys", "sysfs", api_mount_flags | MS_RDONLY, NULL) < 0 ? -errno : 0;
    network_leave_guest();
    if (rc < 0)
        return rc;
    if (mount("securityfs", GUEST_ROOT "/sys/kernel/security", "securityfs", api_mount_flags | MS_RDONLY, NULL) < 0 ||
        mount("fusectl", GUEST_ROOT "/sys/fs/fuse/connections", "fusectl", api_mount_flags | MS_RDONLY, NULL) < 0)
        return -errno;
    return 0;
}

static int assemble_run(void) {
    if (mount("tmpfs", GUEST_ROOT "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755") < 0)
        return -errno;
    static const struct {
        const char *path;
        mode_t mode;
    } dirs[] = {
        {GUEST_ROOT "/run/lock", 01777},
        {GUEST_ROOT "/run/user", 0755},
        {GUEST_ROOT "/run/user/0", 0700},
        {GUEST_ROOT GUEST_RUN_HOST, 0755},
        {GUEST_ROOT "/run/systemd", 0755},
        {GUEST_ROOT "/run/systemd/network", 0755},
        {SYSTEMD_CONTROL, 0755},
        {SYSTEMD_CONTROL "/multi-user.target.wants", 0755},
        {SYSTEMD_CONTROL "/rescue.target.wants", 0755},
        {SYSTEMD_CONTROL "/emergency.target.wants", 0755},
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        if (mkdir(dirs[i].path, dirs[i].mode) < 0)
            return -errno;
    }
    static const char sessions_unit[] =
        "[Unit]\n"
        "Description=Swipterm terminal sessions\n"
        "DefaultDependencies=no\n"
        "Conflicts=shutdown.target\n"
        "Before=shutdown.target\n"
        "After=sysinit.target systemd-tmpfiles-setup.service systemd-user-sessions.service\n"
        "\n"
        "[Service]\n"
        "ExecStart=" GUEST_TOOL_PATH " " SESSIONS_ANCHOR_COMMAND "\n"
        "Delegate=yes\n";
    static const char fallback_network[] =
        "[Match]\n"
        "Name=eth0\n"
        "\n"
        "[Network]\n"
        "DHCP=ipv4\n"
        "IPv6AcceptRA=yes\n"
        "KeepConfiguration=static\n"
        "DNS=10.0.2.3\n"
        "DNS=fd00::3\n";
    const int create = O_CREAT | O_EXCL;
    int rc = write_file(AT_FDCWD, GUEST_ROOT GUEST_RUN_HOST "/container-manager", "lxc\n", create, 0444);
    if (rc == 0)
        rc = write_file(AT_FDCWD, GUEST_ROOT GUEST_RUN_HOST "/resolv.conf", GUEST_DEFAULT_RESOLV_CONF, create, 0644);
    if (rc == 0)
        rc = write_file(AT_FDCWD, GUEST_ROOT GUEST_RUN_HOST "/swiptermd", "", create, 0755);
    if (rc == 0)
        rc = write_file(AT_FDCWD, SYSTEMD_CONTROL "/" SESSIONS_UNIT, sessions_unit, create, 0644);
    if (rc == 0)
        rc = write_file(AT_FDCWD, GUEST_ROOT "/run/systemd/network/99-swipterm-eth0.network", fallback_network,
                        create, 0644);
    if (rc < 0)
        return rc;
    if (symlink("../" SESSIONS_UNIT, SYSTEMD_CONTROL "/multi-user.target.wants/" SESSIONS_UNIT) < 0 ||
        symlink("../" SESSIONS_UNIT, SYSTEMD_CONTROL "/rescue.target.wants/" SESSIONS_UNIT) < 0 ||
        symlink("../" SESSIONS_UNIT, SYSTEMD_CONTROL "/emergency.target.wants/" SESSIONS_UNIT) < 0)
        return -errno;
    if (mount("/init", GUEST_ROOT GUEST_TOOL_PATH, NULL, MS_BIND, NULL) < 0 ||
        mount(NULL, GUEST_ROOT GUEST_TOOL_PATH, NULL, MS_BIND | MS_REMOUNT | MS_RDONLY | MS_NODEV, NULL) < 0)
        return -errno;
    return 0;
}

int container_assemble(void) {
    static const struct {
        const char *path;
        mode_t mode;
    } mountpoints[] = {
        {GUEST_ROOT "/proc", 0555},
        {GUEST_ROOT "/sys", 0555},
        {GUEST_ROOT "/dev", 0755},
        {GUEST_ROOT "/run", 0755},
        {GUEST_ROOT "/tmp", 01777},
    };
    for (size_t i = 0; i < sizeof(mountpoints) / sizeof(mountpoints[0]); i++) {
        int rc = make_mountpoint(mountpoints[i].path, mountpoints[i].mode);
        if (rc < 0)
            return rc;
    }
    int rc = assemble_dev();
    if (rc == 0)
        rc = assemble_sys();
    if (rc == 0 && mount("tmpfs", GUEST_ROOT "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777") < 0)
        rc = -errno;
    if (rc == 0)
        rc = assemble_run();
    return rc;
}

int container_enter(void) {
    if (setns(network_guest_fd(), CLONE_NEWNET) < 0)
        return -errno;
    int rc = write_file(AT_FDCWD, CGROUP_MACHINE "/cgroup.procs", "0", 0, 0);
    if (rc < 0)
        return rc;
    return unshare(CLONE_NEWCGROUP) < 0 ? -errno : 0;
}

int container_mount_api(void) {
    if (mount(NULL, "/", NULL, MS_REC | MS_SLAVE, NULL) < 0 ||
        mount("proc", "/proc", "proc", api_mount_flags, NULL) < 0 ||
        mount("cgroup2", CGROUP_ROOT, "cgroup2", api_mount_flags, NULL) < 0 ||
        mount("mqueue", "/dev/mqueue", "mqueue", api_mount_flags, NULL) < 0 ||
        mount(NULL, "/", NULL, MS_REC | MS_SHARED, NULL) < 0)
        return -errno;
    return 0;
}

int sessions_procs_fd(void) {
    int fd;
    if (g_sessions_dir >= 0) {
        fd = openat(g_sessions_dir, "cgroup.procs", O_WRONLY | O_CLOEXEC);
    } else if (g_daemon.init_mode == SWIPTERMD_INIT_PROCESS && ns_init_is_systemd()) {
        return -EAGAIN;
    } else {
        if (mkdir(CGROUP_PLAIN_SESSIONS, 0755) < 0 && errno != EEXIST)
            return -errno;
        fd = open(CGROUP_PLAIN_SESSIONS "/cgroup.procs", O_WRONLY | O_CLOEXEC);
    }
    return fd < 0 ? -errno : fd;
}

void sessions_register(pid_t sender) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cgroup", (int)sender);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return;
    char membership[PATH_MAX];
    ssize_t n = read(fd, membership, sizeof(membership) - 1);
    close(fd);
    if (n <= 0)
        return;
    membership[n] = '\0';
    char *line = strstr(membership, "0::");
    if (!line || (line != membership && line[-1] != '\n'))
        return;
    char *group = line + 3;
    group[strcspn(group, "\n")] = '\0';
    size_t len = strlen(group);
    static const char agent[] = "/agent";
    if (strncmp(group, "/machine/", 9) != 0 || len < sizeof(agent) ||
        strcmp(group + len - (sizeof(agent) - 1), agent) != 0)
        return;
    group[len - (sizeof(agent) - 1)] = '\0';
    char leaf[PATH_MAX];
    if (snprintf(leaf, sizeof(leaf), CGROUP_ROOT "%s/sessions", group) >= (int)sizeof(leaf))
        return;
    int dir = open(leaf, O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (dir < 0)
        return;
    if (g_sessions_dir >= 0)
        close(g_sessions_dir);
    g_sessions_dir = dir;
    ready_announce(SWIPTERMD_READY_LOGIN);
}

void sessions_shutdown(void) {
    if (g_sessions_dir >= 0) {
        close(g_sessions_dir);
        g_sessions_dir = -1;
    }
}
