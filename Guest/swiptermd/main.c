#include "swiptermd.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/random.h>
#include <linux/vm_sockets.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/random.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

daemon_state_t g_daemon = {
    .epfd = -1,
    .ctrl_fd = -1,
    .data_listen_fd = -1,
    .root_fd = -1,
    .next_req_id = 1,
    .container_init_pid = -1,
    .container_pid_ns_fd = -1,
    .container_mnt_ns_fd = -1,
    .container_uts_ns_fd = -1,
    .container_ipc_ns_fd = -1,
    .container_cgroup_ns_fd = -1,
};

#define BOOT_MODULES_LIST "/swipterm/modules.load"
#define BOOT_MODULES_DIR "/swipterm/modules"

enum {
    NSEC_PER_SEC = 1000000000,
    STOP_GRACE_SEC = 20,
    REAP_GRACE_MS = 5000,
    FINAL_REPORT_TIMEOUT_SEC = 2,
};

typedef struct ctrl_tx_buf {
    uint8_t *data;
    size_t len;
    size_t cap;
    size_t off;
} ctrl_tx_buf_t;

typedef struct ctrl_rx_buf {
    uint8_t hdr[SWIPTERMD_FRAME_HEADER_SIZE];
    size_t hdr_got;
    swiptermd_frame_hdr_t cur;
    uint8_t *body;
    size_t body_cap;
    size_t body_got;
} ctrl_rx_buf_t;

static ev_node_t g_ctrl_ev;
static ev_node_t g_data_listen_ev;
static ev_node_t g_sig_ev;
static ev_node_t g_stop_timer_ev;
static ctrl_tx_buf_t g_ctrl_tx;
static ctrl_rx_buf_t g_ctrl_rx;
static struct epoll_event g_ep_events[64];
static int g_ep_count;
static int g_ep_index;

int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -errno;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -errno;
    return 0;
}

int write_file(int dir_fd, const char *path, const char *text, int flags, mode_t mode) {
    int fd = openat(dir_fd, path, O_WRONLY | O_CLOEXEC | flags, mode);
    if (fd < 0)
        return -errno;
    int rc = write_all_blocking(fd, text, strlen(text));
    if (close(fd) < 0 && rc == 0)
        rc = -errno;
    return rc;
}

void ensure_dir(const char *path, mode_t mode) {
    if (mkdir(path, mode) < 0 && errno != EEXIST)
        return;
    chmod(path, mode);
}

void console_log(const char *format, ...) {
    int fd = open("/dev/console", O_WRONLY | O_NOCTTY | O_CLOEXEC);
    if (fd < 0)
        return;
    char msg[512];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    if (n > 0)
        (void)write(fd, msg, (size_t)n < sizeof(msg) ? (size_t)n : sizeof(msg) - 1);
    close(fd);
}

void boot_err_log(const char *step, int rc) {
    if (rc < 0)
        console_log("swiptermd: %s failed: %s\n", step, strerror(-rc));
    else
        console_log("swiptermd: %s failed with status %d\n", step, rc);
}

int write_all_blocking(int fd, const void *buf, size_t len) {
    const uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

int read_all_blocking(int fd, void *buf, size_t len) {
    uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        if (n == 0)
            return -EPIPE;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static void ev_drop_pending(ev_node_t *node) {
    for (int i = g_ep_index + 1; i < g_ep_count; i++) {
        if (g_ep_events[i].data.ptr == node)
            g_ep_events[i].data.ptr = NULL;
    }
}

int ev_add(ev_node_t *node, int fd, uint32_t events, ev_cb_t cb, void *ctx) {
    node->fd = fd;
    node->events = events;
    node->cb = cb;
    node->ctx = ctx;
    struct epoll_event ev = {.events = events, .data.ptr = node};
    if (epoll_ctl(g_daemon.epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
        return -errno;
    if (events == 0)
        epoll_ctl(g_daemon.epfd, EPOLL_CTL_DEL, fd, NULL);
    node->registered = true;
    return 0;
}

int ev_mod(ev_node_t *node, uint32_t events) {
    if (!node->registered)
        return -ENOENT;
    if (node->events == events)
        return 0;
    int op = events == 0 ? EPOLL_CTL_DEL : node->events == 0 ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    struct epoll_event ev = {.events = events, .data.ptr = node};
    if (epoll_ctl(g_daemon.epfd, op, node->fd, &ev) < 0)
        return -errno;
    if (events == 0)
        ev_drop_pending(node);
    node->events = events;
    return 0;
}

void ev_del(ev_node_t *node) {
    ev_drop_pending(node);
    if (!node->registered)
        return;
    if (node->events != 0)
        epoll_ctl(g_daemon.epfd, EPOLL_CTL_DEL, node->fd, NULL);
    node->registered = false;
    node->events = 0;
}

uint32_t ctrl_alloc_req_id(void) {
    uint32_t id = g_daemon.next_req_id++;
    if (id == 0)
        id = g_daemon.next_req_id++;
    return id;
}

static void ctrl_tx_compact(void) {
    if (g_ctrl_tx.off == 0)
        return;
    size_t rem = g_ctrl_tx.len - g_ctrl_tx.off;
    if (rem > 0)
        memmove(g_ctrl_tx.data, g_ctrl_tx.data + g_ctrl_tx.off, rem);
    g_ctrl_tx.len = rem;
    g_ctrl_tx.off = 0;
}

static int ctrl_tx_flush(void) {
    while (g_ctrl_tx.off < g_ctrl_tx.len) {
        ssize_t n = send(g_daemon.ctrl_fd, g_ctrl_tx.data + g_ctrl_tx.off,
                         g_ctrl_tx.len - g_ctrl_tx.off, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return -errno;
        }
        g_ctrl_tx.off += (size_t)n;
    }
    if (g_ctrl_tx.off == g_ctrl_tx.len) {
        g_ctrl_tx.off = 0;
        g_ctrl_tx.len = 0;
        ev_mod(&g_ctrl_ev, EPOLLIN);
    } else {
        if (g_ctrl_tx.off > 65536)
            ctrl_tx_compact();
        ev_mod(&g_ctrl_ev, EPOLLIN | EPOLLOUT);
    }
    return 0;
}

int ctrl_send_frame(uint16_t opcode, uint32_t req_id, const void *body, uint32_t body_len) {
    if (g_daemon.ctrl_fd < 0 || body_len > SWIPTERMD_MAX_BODY_LEN)
        return -EINVAL;
    size_t total = SWIPTERMD_FRAME_HEADER_SIZE + (size_t)body_len;
    if (g_ctrl_tx.len + total > g_ctrl_tx.cap) {
        ctrl_tx_compact();
        if (g_ctrl_tx.len + total > g_ctrl_tx.cap) {
            size_t ncap = g_ctrl_tx.cap ? g_ctrl_tx.cap * 2 : 65536;
            while (ncap < g_ctrl_tx.len + total)
                ncap *= 2;
            uint8_t *nb = realloc(g_ctrl_tx.data, ncap);
            if (!nb)
                return -ENOMEM;
            g_ctrl_tx.data = nb;
            g_ctrl_tx.cap = ncap;
        }
    }
    swiptermd_frame_hdr_t hdr = {
        .body_len = body_len,
        .version = SWIPTERMD_PROTO_VERSION,
        .opcode = opcode,
        .req_id = req_id,
    };
    swiptermd_frame_hdr_encode(g_ctrl_tx.data + g_ctrl_tx.len, &hdr);
    g_ctrl_tx.len += SWIPTERMD_FRAME_HEADER_SIZE;
    if (body_len > 0) {
        memcpy(g_ctrl_tx.data + g_ctrl_tx.len, body, body_len);
        g_ctrl_tx.len += body_len;
    }
    return ctrl_tx_flush();
}

int ctrl_send_err_reply(uint16_t opcode, uint32_t req_id, int32_t err) {
    uint8_t buf[SWIPTERMD_ERR_REPLY_SIZE];
    swiptermd_enc_err(buf, err);
    return ctrl_send_frame((uint16_t)(opcode | SWIPTERMD_OP_REPLY_FLAG), req_id, buf, sizeof(buf));
}

int vsock_connect_stream(uint32_t role, uint32_t resource_id, uint64_t token) {
    int fd = socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -errno;
    struct sockaddr_vm addr;
    memset(&addr, 0, sizeof(addr));
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = SWIPTERMD_HOST_CID;
    addr.svm_port = SWIPTERMD_DATA_PORT;

    while (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        if (errno == EINTR)
            continue;
        int err = -errno;
        close(fd);
        return err;
    }

    swiptermd_attach_hdr_t ah = {
        .session_nonce = g_daemon.session_nonce,
        .role = role,
        .resource_id = resource_id,
        .token = token,
    };
    uint8_t wire[SWIPTERMD_ATTACH_HEADER_SIZE];
    swiptermd_attach_hdr_encode(wire, &ah);
    int err = write_all_blocking(fd, wire, sizeof(wire));
    if (err < 0) {
        close(fd);
        return err;
    }
    err = set_nonblock(fd);
    if (err < 0) {
        close(fd);
        return err;
    }
    return fd;
}


static void raise_nofile_limit(void) {
    FILE *f = fopen("/proc/sys/fs/nr_open", "re");
    if (!f)
        return;
    unsigned long nr_open = 0;
    bool ok = fscanf(f, "%lu", &nr_open) == 1;
    fclose(f);
    struct rlimit limit;
    if (ok && getrlimit(RLIMIT_NOFILE, &limit) == 0) {
        limit.rlim_max = nr_open;
        (void)setrlimit(RLIMIT_NOFILE, &limit);
    }
}

static void setup_initramfs_mounts(void) {
    ensure_dir("/proc", 0555);
    ensure_dir("/sys", 0555);
    ensure_dir("/dev", 0755);
    ensure_dir("/run", 0755);
    ensure_dir("/mnt", 0755);
    ensure_dir(GUEST_ROOT, 0755);

    (void)mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
    (void)mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
    (void)mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, "mode=0755");
    ensure_dir("/dev/pts", 0755);
    ensure_dir("/dev/shm", 01777);
    (void)mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "gid=5,mode=0620,ptmxmode=0666");
    (void)mount("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777");
    (void)mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755");
}

static int load_module(const char *path) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    int rc = syscall(SYS_finit_module, fd, "", 0) < 0 && errno != EEXIST ? -errno : 0;
    close(fd);
    (void)unlink(path);
    return rc;
}

static int load_boot_modules(void) {
    FILE *list = fopen(BOOT_MODULES_LIST, "re");
    if (!list) {
        if (errno == ENOENT)
            return 0;
        int err = -errno;
        boot_err_log(BOOT_MODULES_LIST, err);
        return err;
    }
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    int rc = 0;
    while (rc == 0 && (len = getline(&line, &cap, list)) >= 0) {
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';
        if (len == 0)
            continue;
        if (len < 3 || (line[0] != 'R' && line[0] != 'O') || line[1] != ' ') {
            rc = -EINVAL;
            boot_err_log(BOOT_MODULES_LIST, rc);
            break;
        }
        const char *name = line + 2;
        char path[PATH_MAX];
        if (snprintf(path, sizeof(path), name[0] == '/' ? "%s" : BOOT_MODULES_DIR "/%s", name) >=
            (int)sizeof(path)) {
            rc = -ENAMETOOLONG;
            boot_err_log(name, rc);
            break;
        }
        int err = load_module(path);
        if (err < 0) {
            boot_err_log(path, err);
            if (line[0] == 'R')
                rc = err;
        }
    }
    if (rc == 0 && ferror(list)) {
        rc = -EIO;
        boot_err_log(BOOT_MODULES_LIST, rc);
    }
    free(line);
    fclose(list);
    (void)unlink(BOOT_MODULES_LIST);
    return rc;
}

static int set_realtime_ns(uint64_t realtime_ns) {
    struct timespec now = {
        .tv_sec = (time_t)(realtime_ns / NSEC_PER_SEC),
        .tv_nsec = (long)(realtime_ns % NSEC_PER_SEC),
    };
    return clock_settime(CLOCK_REALTIME, &now) < 0 ? -errno : 0;
}

typedef struct boot_seed {
    int entropy_count;
    int buf_size;
    uint8_t data[32];
} boot_seed_t;

static int credit_boot_seed(const boot_seed_t *seed) {
    int fd = open("/dev/random", O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    int rc = ioctl(fd, RNDADDENTROPY, seed) < 0 ? -errno : 0;
    if (rc == 0 && ioctl(fd, RNDRESEEDCRNG, 0) < 0 && errno != EINVAL && errno != ENODATA && errno != ENOTTY)
        rc = -errno;
    close(fd);
    return rc;
}

static __attribute__((noinline)) void read_host_boot_info(uint64_t *realtime_ns, uint64_t seed[4]) {
    uint64_t x0, x1, x2, x3, x4;
    __asm__ volatile("mov x0, xzr\n\t"
                     "mov x1, xzr\n\t"
                     "mov x2, xzr\n\t"
                     "mov x3, xzr\n\t"
                     "mov x4, xzr\n\t"
                     "hvc #0x5357\n\t"
                     "mov %0, x0\n\t"
                     "mov %1, x1\n\t"
                     "mov %2, x2\n\t"
                     "mov %3, x3\n\t"
                     "mov %4, x4"
                     : "=r"(x0), "=r"(x1), "=r"(x2), "=r"(x3), "=r"(x4)
                     :
                     : "x0", "x1", "x2", "x3", "x4", "memory");
    *realtime_ns = x0;
    seed[0] = x1;
    seed[1] = x2;
    seed[2] = x3;
    seed[3] = x4;
}

static int apply_host_boot_info(void) {
    uint64_t realtime_ns;
    uint64_t words[4];
    read_host_boot_info(&realtime_ns, words);

    boot_seed_t seed = {.entropy_count = 8 * sizeof(seed.data), .buf_size = sizeof(seed.data)};
    bool has_seed = (words[0] | words[1] | words[2] | words[3]) != 0;
    memcpy(seed.data, words, sizeof(seed.data));
    explicit_bzero(words, sizeof(words));

    int rc = realtime_ns != 0 ? set_realtime_ns(realtime_ns) : 0;
    if (rc < 0)
        boot_err_log("set the realtime clock", rc);
    int seed_rc = has_seed ? credit_boot_seed(&seed) : 0;
    explicit_bzero(seed.data, sizeof(seed.data));
    if (seed_rc < 0)
        boot_err_log("seed the random pool", seed_rc);
    return rc < 0 ? rc : seed_rc;
}

static int bind_boot_virtio(unsigned addr) {
    char name[32];
    int len = snprintf(name, sizeof(name), "%x.virtio_mmio", addr);
    char driver[80];
    snprintf(driver, sizeof(driver), "/sys/bus/platform/devices/%s/driver", name);
    if (access(driver, F_OK) == 0)
        return 0;
    int fd = open("/sys/bus/platform/drivers/virtio-mmio/bind", O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    ssize_t n = write(fd, name, (size_t)len);
    int rc = 0;
    if (n < 0) {
        rc = (errno == EBUSY) ? 0 : -errno;
    } else if (n != len) {
        rc = -EIO;
    }
    close(fd);
    return rc;
}

static bool has_render_node(void) {
    DIR *dir = opendir("/sys/class/drm");
    if (!dir)
        return false;
    bool found = false;
    struct dirent *entry;
    while (!found && (entry = readdir(dir)) != NULL)
        found = strncmp(entry->d_name, "renderD", 7) == 0;
    closedir(dir);
    return found;
}

static int reseed_boot_random(void) {
    int fd = open("/dev/hwrng", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return errno == ENOENT || errno == ENODEV ? 0 : -errno;
    boot_seed_t seed = {.entropy_count = 8 * sizeof(seed.data), .buf_size = sizeof(seed.data)};
    size_t count = 0;
    int rc = 0;
    int retries = 0;
    while (count < sizeof(seed.data)) {
        ssize_t n = read(fd, seed.data + count, sizeof(seed.data) - count);
        if (n < 0 && errno == EINTR)
            continue;
        if ((n < 0 && (errno == ENODATA || errno == EAGAIN)) || n == 0) {
            if (++retries <= 200) {
                usleep(1000);
                continue;
            }
            rc = n < 0 ? -errno : -EIO;
            break;
        }
        if (n < 0) {
            rc = errno == ENODEV ? 0 : -errno;
            break;
        }
        retries = 0;
        count += (size_t)n;
    }
    close(fd);
    if (rc == 0 && count == sizeof(seed.data))
        rc = credit_boot_seed(&seed);
    explicit_bzero(seed.data, sizeof(seed.data));
    return rc;
}

static int setup_boot_devices(void) {
    int rc = bind_boot_virtio(0x0a000800);
    if (rc == -ENODEV && access("/sys/bus/virtio/drivers/virtio_rng", F_OK) != 0)
        rc = 0;
    if (rc < 0)
        return rc;
    rc = reseed_boot_random();
    if (rc < 0)
        return rc;
    for (unsigned addr = 0x0a000000; addr < 0x0a000800; addr += 0x200) {
        rc = bind_boot_virtio(addr);
        if (rc < 0)
            return rc;
    }
    g_daemon.gpu = bind_boot_virtio(0x0a001a00) == 0 && has_render_node();
    return 0;
}

static bool virtiofs_available(void) {
    FILE *filesystems = fopen("/proc/filesystems", "re");
    if (!filesystems)
        return false;
    char line[128];
    bool found = false;
    while (!found && fgets(line, sizeof(line), filesystems)) {
        line[strcspn(line, "\n")] = '\0';
        const char *name = strrchr(line, '\t');
        found = strcmp(name ? name + 1 : line, "virtiofs") == 0;
    }
    fclose(filesystems);
    return found;
}

void ready_announce(uint32_t level) {
    uint32_t bit = 1U << level;
    if (g_daemon.ready_mask & bit)
        return;
    g_daemon.ready_mask |= bit;
    uint8_t msg[SWIPTERMD_READY_NOTIFY_SIZE];
    swiptermd_enc_u32_notify(msg, level);
    ctrl_send_frame(SWIPTERMD_OP_READY, 0, msg, sizeof(msg));
}

static void ctrl_flush_blocking(void) {
    if (g_daemon.ctrl_fd < 0 || g_ctrl_tx.off >= g_ctrl_tx.len)
        return;
    struct timeval timeout = {.tv_sec = FINAL_REPORT_TIMEOUT_SEC};
    (void)setsockopt(g_daemon.ctrl_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    int flags = fcntl(g_daemon.ctrl_fd, F_GETFL, 0);
    if (flags >= 0)
        (void)fcntl(g_daemon.ctrl_fd, F_SETFL, flags & ~O_NONBLOCK);
    (void)write_all_blocking(g_daemon.ctrl_fd, g_ctrl_tx.data + g_ctrl_tx.off, g_ctrl_tx.len - g_ctrl_tx.off);
    g_ctrl_tx.off = g_ctrl_tx.len = 0;
}

static void on_stop_deadline(ev_node_t *node, uint32_t events) {
    (void)events;
    uint64_t expirations;
    (void)read(node->fd, &expirations, sizeof(expirations));
    if (g_daemon.container_init_pid > 0)
        g_daemon.stop_flags |= SWIPTERMD_STOP_FORCED;
    g_daemon.shutdown_requested = true;
}

void shutdown_check_done(void) {
    if (g_daemon.stopping && g_daemon.container_init_pid <= 0)
        g_daemon.shutdown_requested = true;
}

void shutdown_begin(bool reboot) {
    if (g_daemon.stopping)
        return;
    g_daemon.stopping = true;
    g_daemon.reboot_requested = reboot;
    if (g_daemon.container_init_pid <= 0) {
        g_daemon.shutdown_requested = true;
        return;
    }
    int timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    struct itimerspec deadline = {.it_value = {.tv_sec = STOP_GRACE_SEC}};
    if (timer < 0 || timerfd_settime(timer, 0, &deadline, NULL) < 0 ||
        ev_add(&g_stop_timer_ev, timer, EPOLLIN, on_stop_deadline, NULL) < 0) {
        boot_err_log("arm the shutdown deadline", -errno);
        if (timer >= 0)
            close(timer);
        g_daemon.stop_flags |= SWIPTERMD_STOP_FORCED;
        g_daemon.shutdown_requested = true;
        return;
    }
    ns_request_stop(reboot);
}

static void handle_host_halt(uint32_t req_id, const uint8_t *body, uint32_t len) {
    uint32_t flags = 0;
    (void)swiptermd_dec_halt_req(body, len, &flags);
    ctrl_send_err_reply(SWIPTERMD_OP_HALT, req_id, 0);
    shutdown_begin(flags == 1);
}

static void reap_children(int timeout_ms) {
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0)
            continue;
        if (pid < 0 && errno == EINTR)
            continue;
        if (pid < 0)
            return;
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed >= timeout_ms || !g_sig_ev.registered) {
            boot_err_log("wait for guest helpers", -ETIMEDOUT);
            return;
        }
        struct pollfd pfd = {.fd = g_sig_ev.fd, .events = POLLIN};
        if (poll(&pfd, 1, (int)(timeout_ms - elapsed)) > 0) {
            struct signalfd_siginfo si;
            while (read(g_sig_ev.fd, &si, sizeof(si)) > 0) {}
        }
    }
}

static void finish(void) {
    if (g_daemon.container_init_pid > 0) {
        g_daemon.stop_flags |= SWIPTERMD_STOP_FORCED;
        ns_kill_container();
    }
    proc_pty_shutdown();
    host_cmd_cuse_shutdown();
    clipboard_shutdown();
    notify_close();
    sessions_shutdown();
    reap_children(REAP_GRACE_MS);
    ns_close_namespaces();
    if (g_daemon.root_fd >= 0) {
        close(g_daemon.root_fd);
        g_daemon.root_fd = -1;
    }
    fs_unmount_all();
    if (!storage_teardown())
        g_daemon.stop_flags |= SWIPTERMD_STOP_STORAGE_ERROR;
    if (g_daemon.ctrl_fd >= 0) {
        uint8_t msg[SWIPTERMD_STOPPED_NOTIFY_SIZE];
        swiptermd_enc_u32_notify(msg, g_daemon.stop_flags);
        ctrl_send_frame(SWIPTERMD_OP_STOPPED, 0, msg, sizeof(msg));
        ctrl_flush_blocking();
    }
    reboot(g_daemon.reboot_requested ? RB_AUTOBOOT : RB_POWER_OFF);
}

static void on_signal(ev_node_t *node, uint32_t events) {
    (void)events;
    struct signalfd_siginfo si;
    while (read(node->fd, &si, sizeof(si)) > 0) {
        int sig = (int)si.ssi_signo;
        if (sig == SIGPWR || sig == SIGRTMIN + 3 || sig == SIGRTMIN + 4)
            shutdown_begin(false);
        else if (sig == SIGRTMIN + 5)
            shutdown_begin(true);
    }
    for (;;) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid < 0 && errno == EINTR)
            continue;
        if (pid <= 0)
            break;
        if (pid == g_daemon.container_init_pid)
            ns_init_exited(status);
    }
}

static void dispatch_ctrl_frame(const swiptermd_frame_hdr_t *hdr, const uint8_t *body) {
    if (hdr->version != SWIPTERMD_PROTO_VERSION)
        return;

    if (hdr->opcode & SWIPTERMD_OP_REPLY_FLAG) {
        uint16_t base_op = (uint16_t)(hdr->opcode & ~SWIPTERMD_OP_REPLY_FLAG);
        if (base_op == SWIPTERMD_OP_HELLO)
            return;
        if (base_op == SWIPTERMD_OP_CMD_START) {
            host_cmd_handle_start_reply(hdr->req_id, body, hdr->body_len);
            return;
        }
        (void)cuse_handle_reply(base_op, hdr->req_id, body, hdr->body_len);
        return;
    }

    switch (hdr->opcode) {
    case SWIPTERMD_OP_CONFIGURE_INIT:
        ns_handle_configure_init(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_START_INIT:
        ns_handle_start_init(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_HALT:
        handle_host_halt(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_SUSPEND:
        sync();
        ctrl_send_err_reply(SWIPTERMD_OP_SUSPEND, hdr->req_id, 0);
        break;
    case SWIPTERMD_OP_RESUME: {
        int64_t realtime_ns = 0;
        int rc = 0;
        if (swiptermd_dec_resume_req(body, hdr->body_len, &realtime_ns) && realtime_ns > 0)
            rc = set_realtime_ns((uint64_t)realtime_ns);
        ctrl_send_err_reply(SWIPTERMD_OP_RESUME, hdr->req_id, rc);
        break;
    }
    case SWIPTERMD_OP_SPAWN:
        proc_handle_spawn(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_LOGIN:
        proc_handle_login(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_PROCESS_SIGNAL:
        proc_handle_signal(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_PTY_CREATE:
        pty_handle_create(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_PTY_RESIZE:
        pty_handle_resize(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_PTY_CLOSE:
        pty_handle_close(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_FS_STAT:
        fs_handle_stat(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_FS_READ:
        fs_handle_read(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_FS_WRITE:
        fs_handle_write(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_FS_LIST:
        fs_handle_list(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_FS_MKDIR:
        fs_handle_mkdir(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_FS_REMOVE:
        fs_handle_remove(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_FS_MOUNT_VIRTIOFS:
        fs_handle_mount_virtiofs(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_FS_UNMOUNT:
        fs_handle_unmount(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_REGISTER_CMD:
        host_cmd_handle_register(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_CMD_FINISH:
        host_cmd_handle_finish(body, hdr->body_len);
        break;
    case SWIPTERMD_OP_REGISTER_DEVICE:
        cuse_handle_register_device(hdr->req_id, body, hdr->body_len);
        break;
    case SWIPTERMD_OP_CLIPBOARD_OFFER:
        clipboard_handle_offer(body, hdr->body_len);
        break;
    default:
        ctrl_send_err_reply(hdr->opcode, hdr->req_id, -ENOSYS);
        break;
    }
}

static void on_ctrl_event(ev_node_t *node, uint32_t events) {
    (void)node;
    if (events & (EPOLLERR | EPOLLHUP)) {
        g_daemon.shutdown_requested = true;
        return;
    }
    if (events & EPOLLOUT) {
        if (ctrl_tx_flush() < 0) {
            g_daemon.shutdown_requested = true;
            return;
        }
    }
    if (!(events & EPOLLIN))
        return;

    for (;;) {
        if (g_ctrl_rx.hdr_got < SWIPTERMD_FRAME_HEADER_SIZE) {
            ssize_t n = recv(g_daemon.ctrl_fd, g_ctrl_rx.hdr + g_ctrl_rx.hdr_got,
                             SWIPTERMD_FRAME_HEADER_SIZE - g_ctrl_rx.hdr_got, 0);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return;
                g_daemon.shutdown_requested = true;
                return;
            }
            if (n == 0) {
                g_daemon.shutdown_requested = true;
                return;
            }
            g_ctrl_rx.hdr_got += (size_t)n;
            if (g_ctrl_rx.hdr_got < SWIPTERMD_FRAME_HEADER_SIZE)
                return;

            swiptermd_frame_hdr_decode(g_ctrl_rx.hdr, &g_ctrl_rx.cur);
            if (g_ctrl_rx.cur.body_len > SWIPTERMD_MAX_BODY_LEN) {
                g_daemon.shutdown_requested = true;
                return;
            }
            if (g_ctrl_rx.cur.body_len > g_ctrl_rx.body_cap) {
                uint8_t *nb = realloc(g_ctrl_rx.body, g_ctrl_rx.cur.body_len);
                if (!nb) {
                    g_daemon.shutdown_requested = true;
                    return;
                }
                g_ctrl_rx.body = nb;
                g_ctrl_rx.body_cap = g_ctrl_rx.cur.body_len;
            }
            g_ctrl_rx.body_got = 0;
        }

        while (g_ctrl_rx.body_got < g_ctrl_rx.cur.body_len) {
            ssize_t n = recv(g_daemon.ctrl_fd, g_ctrl_rx.body + g_ctrl_rx.body_got,
                             g_ctrl_rx.cur.body_len - g_ctrl_rx.body_got, 0);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return;
                g_daemon.shutdown_requested = true;
                return;
            }
            if (n == 0) {
                g_daemon.shutdown_requested = true;
                return;
            }
            g_ctrl_rx.body_got += (size_t)n;
        }

        swiptermd_frame_hdr_t frame = g_ctrl_rx.cur;
        g_ctrl_rx.hdr_got = 0;
        g_ctrl_rx.body_got = 0;
        dispatch_ctrl_frame(&frame, g_ctrl_rx.body);
    }
}

static void on_data_accept(ev_node_t *node, uint32_t events) {
    (void)node;
    if (!(events & EPOLLIN))
        return;

    for (;;) {
        int cfd = accept4(g_daemon.data_listen_fd, NULL, NULL, SOCK_CLOEXEC);
        if (cfd < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        uint8_t wire[SWIPTERMD_ATTACH_HEADER_SIZE];
        if (read_all_blocking(cfd, wire, sizeof(wire)) < 0) {
            close(cfd);
            continue;
        }
        swiptermd_attach_hdr_t ah;
        swiptermd_attach_hdr_decode(wire, &ah);
        if (ah.session_nonce != g_daemon.session_nonce) {
            close(cfd);
            continue;
        }
        if (ah.role == SWIPTERMD_STREAM_TCP_MIRROR) {
            if (set_nonblock(cfd) < 0) {
                close(cfd);
                continue;
            }
            sock_mirror_handle_incoming(cfd, ah.resource_id);
        } else {
            close(cfd);
        }
    }
}

static int connect_control_vsock(void) {
    for (int attempt = 0; attempt < 200; attempt++) {
        int fd = socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0)
            return -errno;
        struct sockaddr_vm addr;
        memset(&addr, 0, sizeof(addr));
        addr.svm_family = AF_VSOCK;
        addr.svm_cid = SWIPTERMD_HOST_CID;
        addr.svm_port = SWIPTERMD_CONTROL_PORT;
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
            return fd;
        close(fd);
        usleep(10000);
    }
    return -ETIMEDOUT;
}

static int listen_data_vsock(void) {
    int fd = socket(AF_VSOCK, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -errno;
    struct sockaddr_vm addr;
    memset(&addr, 0, sizeof(addr));
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = VMADDR_CID_ANY;
    addr.svm_port = SWIPTERMD_DATA_PORT;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(fd, 64) < 0) {
        int err = -errno;
        close(fd);
        return err;
    }
    return fd;
}

static int setup_signals(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGPWR);
    sigaddset(&mask, SIGRTMIN + 3);
    sigaddset(&mask, SIGRTMIN + 4);
    sigaddset(&mask, SIGRTMIN + 5);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
        return -errno;
    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd < 0)
        return -errno;
    int rc = ev_add(&g_sig_ev, sfd, EPOLLIN, on_signal, NULL);
    if (rc < 0)
        close(sfd);
    return rc;
}

static int setup_guest(void) {
    static const struct {
        const char *step;
        int (*run)(void);
    } steps[] = {
        {"cgroup setup", cgroup_setup},
        {"mount the root filesystem", storage_mount_root},
        {"network setup", network_setup},
        {"container assembly", container_assemble},
    };
    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        int rc = steps[i].run();
        if (rc < 0) {
            boot_err_log(steps[i].step, rc);
            return rc;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], SESSIONS_ANCHOR_COMMAND) == 0)
        return sessions_anchor_main();
    if (getpid() != 1) {
        fprintf(stderr, "usage: %s " SESSIONS_ANCHOR_COMMAND "\n", argv[0]);
        return 64;
    }
    if (strcmp(argv[0], MANAGED_INIT_NAME) == 0)
        managed_init_main();

    umask(0);
    signal(SIGPIPE, SIG_IGN);

    setup_initramfs_mounts();
    raise_nofile_limit();
    int boot_rc = write_file(AT_FDCWD, "/proc/sys/kernel/modprobe", "/sbin/modprobe\n", O_TRUNC, 0);
    if (boot_rc < 0 && boot_rc != -ENOENT)
        boot_err_log("set the kernel modprobe path", boot_rc);
    if (load_boot_modules() < 0 || apply_host_boot_info() < 0) {
        reboot(RB_POWER_OFF);
        return 1;
    }
    boot_rc = setup_boot_devices();
    if (boot_rc < 0) {
        boot_err_log("setup_boot_devices", boot_rc);
        reboot(RB_POWER_OFF);
        return 1;
    }

    g_daemon.epfd = epoll_create1(EPOLL_CLOEXEC);
    if (g_daemon.epfd < 0 || setup_signals() < 0 || setup_guest() < 0) {
        finish();
        return 1;
    }

    strcpy(g_daemon.host_cmd_dir, "/run/host-bin");
    boot_rc = host_cmd_setup(g_daemon.host_cmd_dir);
    if (boot_rc < 0)
        boot_err_log("host command setup", boot_rc);

    bool clipboard_ok = true;
    boot_rc = clipboard_setup();
    if (boot_rc < 0) {
        boot_err_log("clipboard setup", boot_rc);
        clipboard_ok = false;
    }

    boot_rc = notify_setup();
    if (boot_rc < 0) {
        boot_err_log("notify socket", boot_rc);
        finish();
        return 1;
    }

    if (getrandom(&g_daemon.session_nonce, sizeof(g_daemon.session_nonce), 0) !=
        (ssize_t)sizeof(g_daemon.session_nonce)) {
        boot_err_log("getrandom", -errno);
        finish();
        return 1;
    }

    g_daemon.ctrl_fd = connect_control_vsock();
    if (g_daemon.ctrl_fd < 0) {
        boot_err_log("connect_control_vsock", g_daemon.ctrl_fd);
        g_daemon.ctrl_fd = -1;
        finish();
        return 1;
    }
    (void)set_nonblock(g_daemon.ctrl_fd);
    ev_add(&g_ctrl_ev, g_daemon.ctrl_fd, EPOLLIN, on_ctrl_event, NULL);

    g_daemon.data_listen_fd = listen_data_vsock();
    if (g_daemon.data_listen_fd >= 0)
        ev_add(&g_data_listen_ev, g_daemon.data_listen_fd, EPOLLIN, on_data_accept, NULL);

    proc_pty_init();
    sock_mirror_init();

    uint32_t caps = SWIPTERMD_CAP_READY | SWIPTERMD_CAP_LOGIN | SWIPTERMD_CAP_STOP_STATUS;
    if (cuse_available())
        caps |= SWIPTERMD_CAP_DEVICES;
    if (virtiofs_available())
        caps |= SWIPTERMD_CAP_VIRTIOFS;
    if (clipboard_ok)
        caps |= SWIPTERMD_CAP_CLIPBOARD;
    uint8_t hello[SWIPTERMD_HELLO_REQ_SIZE];
    swiptermd_enc_hello(hello, g_daemon.session_nonce, caps);
    ctrl_send_frame(SWIPTERMD_OP_HELLO, ctrl_alloc_req_id(), hello, sizeof(hello));

    while (!g_daemon.shutdown_requested) {
        int n = epoll_wait(g_daemon.epfd, g_ep_events, 64, -1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        g_ep_count = n;
        for (g_ep_index = 0; g_ep_index < g_ep_count; g_ep_index++) {
            ev_node_t *node = g_ep_events[g_ep_index].data.ptr;
            if (node && node->cb)
                node->cb(node, g_ep_events[g_ep_index].events);
        }
        g_ep_count = 0;
        g_ep_index = 0;
    }

    finish();
    return 0;
}
