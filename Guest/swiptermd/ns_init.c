#include "swiptermd.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CONSOLE_BUF_CAP 32768
#define CONTAINER_TTY_COUNT 4
#define LXC_TEMPLATE_PLACEHOLDER "LXC_NAME"

enum {
    MANAGED_INIT_GRACE_MS = 15000,
    MANAGED_INIT_POLL_NS = 5 * 1000 * 1000,
};

typedef struct console_state {
    int master_fd;
    int slave_guard;
    int vsock_fd;
    ev_node_t master_ev;
    ev_node_t vsock_ev;
    uint8_t buf[CONSOLE_BUF_CAP];
    size_t len;
    size_t off;
} console_state_t;

static console_state_t g_console = {
    .master_fd = -1,
    .slave_guard = -1,
    .vsock_fd = -1,
};

static int g_tty_masters[CONTAINER_TTY_COUNT] = {-1, -1, -1, -1};
static bool g_stop_reboot;

static void ttys_close(void) {
    for (size_t i = 0; i < CONTAINER_TTY_COUNT; i++) {
        if (g_tty_masters[i] >= 0) {
            close(g_tty_masters[i]);
            g_tty_masters[i] = -1;
        }
    }
}

static int ttys_open(void) {
    for (int i = 0; i < CONTAINER_TTY_COUNT; i++) {
        int pts_num = -1;
        int master = pty_open_master(&pts_num);
        if (master < 0)
            return master;
        g_tty_masters[i] = master;
        char source[64];
        char target[64];
        snprintf(source, sizeof(source), GUEST_ROOT "/dev/pts/%d", pts_num);
        snprintf(target, sizeof(target), GUEST_ROOT "/dev/tty%d", i + 1);
        int node = open(target, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0620);
        if (node < 0)
            return -errno;
        close(node);
        if (mount(source, target, NULL, MS_BIND, NULL) < 0)
            return -errno;
    }
    return 0;
}

static void apply_lxc_template(const char *guest_path) {
    struct stat st;
    char *text = fs_read_root_file(guest_path + 1, &st);
    if (!text)
        return;
    size_t n = (size_t)st.st_size;

    const size_t placeholder_len = strlen(LXC_TEMPLATE_PLACEHOLDER);
    const size_t name_len = strlen(g_daemon.hostname);
    size_t count = 0;
    for (const char *p = text; (p = strstr(p, LXC_TEMPLATE_PLACEHOLDER)); p += placeholder_len)
        count++;
    char *out = count > 0 ? malloc(n + count * name_len + 1) : NULL;
    if (out) {
        size_t len = 0;
        const char *cursor = text;
        for (const char *p; (p = strstr(cursor, LXC_TEMPLATE_PLACEHOLDER)); cursor = p + placeholder_len) {
            memcpy(out + len, cursor, (size_t)(p - cursor));
            len += (size_t)(p - cursor);
            memcpy(out + len, g_daemon.hostname, name_len);
            len += name_len;
        }
        size_t rest = strlen(cursor);
        memcpy(out + len, cursor, rest);
        len += rest;
        int rc = fs_write_file_jailed(guest_path, (const uint8_t *)out, len, st.st_mode & 07777, false);
        if (rc < 0)
            boot_err_log(guest_path, rc);
        free(out);
    }
    free(text);
}

static bool hosts_line_names(const char *line, size_t len, const char *name, bool *loopback_alias) {
    char buf[1024];
    if (len >= sizeof(buf))
        return false;
    memcpy(buf, line, len);
    buf[len] = '\0';
    buf[strcspn(buf, "#")] = '\0';
    char *save = NULL;
    char *address = strtok_r(buf, " \t\r", &save);
    if (!address)
        return false;
    *loopback_alias = strcmp(address, "127.0.1.1") == 0;
    for (char *alias = strtok_r(NULL, " \t\r", &save); alias; alias = strtok_r(NULL, " \t\r", &save)) {
        if (strcasecmp(alias, name) == 0)
            return true;
    }
    return false;
}

static void ensure_hosts_entry(void) {
    const char *name = g_daemon.hostname;
    if (name[0] == '\0')
        return;
    struct stat st;
    char *text = fs_read_root_file("etc/hosts", &st);
    if (!text)
        return;
    size_t len = (size_t)st.st_size;

    const char *replace_from = NULL;
    const char *replace_to = NULL;
    for (const char *line = text; line < text + len;) {
        const char *end = memchr(line, '\n', (size_t)(text + len - line));
        size_t line_len = end ? (size_t)(end - line) : (size_t)(text + len - line);
        bool loopback_alias = false;
        if (hosts_line_names(line, line_len, name, &loopback_alias)) {
            free(text);
            return;
        }
        if (loopback_alias && !replace_from) {
            replace_from = line;
            replace_to = line + line_len;
        }
        line += line_len + (end ? 1 : 0);
    }

    char entry[sizeof(g_daemon.hostname) + 16];
    int entry_len = snprintf(entry, sizeof(entry), "127.0.1.1\t%s", name);
    char *out = malloc(len + (size_t)entry_len + 2);
    if (out) {
        size_t out_len = 0;
        if (replace_from) {
            size_t head = (size_t)(replace_from - text);
            memcpy(out, text, head);
            memcpy(out + head, entry, (size_t)entry_len);
            size_t tail = len - (size_t)(replace_to - text);
            memcpy(out + head + (size_t)entry_len, replace_to, tail);
            out_len = head + (size_t)entry_len + tail;
        } else {
            memcpy(out, text, len);
            out_len = len;
            if (out_len > 0 && out[out_len - 1] != '\n')
                out[out_len++] = '\n';
            memcpy(out + out_len, entry, (size_t)entry_len);
            out_len += (size_t)entry_len;
            out[out_len++] = '\n';
        }
        int rc = fs_write_file_jailed("/etc/hosts", (const uint8_t *)out, out_len, st.st_mode & 07777, false);
        if (rc < 0)
            boot_err_log("write /etc/hosts", rc);
        free(out);
    }
    free(text);
}

static void console_close_vsock(void) {
    ev_del(&g_console.vsock_ev);
    if (g_console.vsock_fd >= 0) {
        close(g_console.vsock_fd);
        g_console.vsock_fd = -1;
    }
    g_console.len = 0;
    g_console.off = 0;
}

static void console_close_all(void) {
    console_close_vsock();
    ev_del(&g_console.master_ev);
    if (g_console.master_fd >= 0) {
        close(g_console.master_fd);
        g_console.master_fd = -1;
    }
    if (g_console.slave_guard >= 0) {
        close(g_console.slave_guard);
        g_console.slave_guard = -1;
    }
}

static bool console_flush(void) {
    while (g_console.off < g_console.len) {
        ssize_t n = send(g_console.vsock_fd, g_console.buf + g_console.off, g_console.len - g_console.off,
                         MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return false;
            console_close_vsock();
            return true;
        }
        g_console.off += (size_t)n;
    }
    g_console.off = 0;
    g_console.len = 0;
    return true;
}

static void on_console_event(ev_node_t *node, uint32_t events) {
    (void)node;
    (void)events;
    if (g_console.master_fd < 0)
        return;
    for (;;) {
        if (g_console.vsock_fd >= 0 && !console_flush()) {
            ev_mod(&g_console.master_ev, 0);
            ev_mod(&g_console.vsock_ev, EPOLLOUT);
            return;
        }
        ssize_t n = read(g_console.master_fd, g_console.buf, sizeof(g_console.buf));
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        if (n <= 0) {
            ev_del(&g_console.master_ev);
            close(g_console.master_fd);
            g_console.master_fd = -1;
            console_close_vsock();
            return;
        }
        if (g_console.vsock_fd >= 0) {
            g_console.len = (size_t)n;
            g_console.off = 0;
        }
    }
    ev_mod(&g_console.master_ev, EPOLLIN);
    if (g_console.vsock_fd >= 0)
        ev_mod(&g_console.vsock_ev, 0);
}

static void console_drain(void) {
    if (g_console.vsock_fd >= 0 && g_console.master_fd >= 0) {
        struct timeval timeout = {.tv_sec = 1};
        (void)setsockopt(g_console.vsock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        (void)set_nonblock(g_console.vsock_fd, false);
        bool ok = write_all_blocking(g_console.vsock_fd, g_console.buf + g_console.off,
                                     g_console.len - g_console.off) == 0;
        while (ok) {
            ssize_t n = read(g_console.master_fd, g_console.buf, sizeof(g_console.buf));
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                break;
            ok = write_all_blocking(g_console.vsock_fd, g_console.buf, (size_t)n) == 0;
        }
    }
    console_close_all();
}

static int console_open(bool has_console, uint64_t console_token, int *pts_num_out) {
    int pts_num = -1;
    int master = pty_open_master(&pts_num);
    if (master < 0)
        return master;
    int vsock = -1;
    int rc = 0;
    int guard = ioctl(master, TIOCGPTPEER, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (guard < 0)
        rc = -errno;
    if (rc == 0) {
        struct winsize ws = {.ws_row = 24, .ws_col = 80};
        (void)ioctl(master, TIOCSWINSZ, &ws);
        if (has_console) {
            vsock = vsock_connect_stream(SWIPTERMD_STREAM_CONSOLE, 0, console_token);
            if (vsock < 0)
                rc = vsock;
        }
    }
    if (rc == 0)
        rc = ev_add(&g_console.master_ev, master, EPOLLIN, on_console_event, NULL);
    if (rc == 0 && vsock >= 0) {
        rc = ev_add(&g_console.vsock_ev, vsock, 0, on_console_event, NULL);
        if (rc < 0)
            ev_del(&g_console.master_ev);
    }
    if (rc < 0) {
        if (vsock >= 0)
            close(vsock);
        if (guard >= 0)
            close(guard);
        close(master);
        return rc;
    }
    g_console.master_fd = master;
    g_console.slave_guard = guard;
    g_console.vsock_fd = vsock;
    g_console.len = 0;
    g_console.off = 0;
    *pts_num_out = pts_num;
    return 0;
}

void ns_handle_configure_init(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = {.data = body, .len = len};
    char hostname[256];
    char cmd_dir[512];

    if (!swiptermd_r_str_copy(&r, hostname, sizeof(hostname)) ||
        !swiptermd_r_str_copy(&r, cmd_dir, sizeof(cmd_dir))) {
        ctrl_send_err_reply(SWIPTERMD_OP_CONFIGURE_INIT, req_id, -EINVAL);
        return;
    }

    if (hostname[0] != '\0')
        strcpy(g_daemon.hostname, hostname);
    int err = 0;
    if (cmd_dir[0] != '\0') {
        strcpy(g_daemon.host_cmd_dir, cmd_dir);
        err = host_cmd_setup(g_daemon.host_cmd_dir);
    }
    ctrl_send_err_reply(SWIPTERMD_OP_CONFIGURE_INIT, req_id, err);
}

typedef struct init_child_args {
    uint32_t mode;
    const char *hostname;
    const char *path;
    char **argv;
    char **envp;
    int pts_num;
    int ready_wr;
    int go_rd;
    int err_wr;
} init_child_args_t;

static void managed_init_signals(sigset_t *set) {
    static const int faults[] = {SIGILL, SIGABRT, SIGFPE, SIGSEGV, SIGBUS, SIGSYS, SIGTRAP};
    sigfillset(set);
    for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); i++)
        sigdelset(set, faults[i]);
}

static bool managed_init_poweroff_signal(int sig) {
    return sig == SIGTERM || sig == SIGPWR || sig == SIGUSR1 || sig == SIGUSR2 || sig == SIGRTMIN + 3 ||
           sig == SIGRTMIN + 4;
}

static _Noreturn void managed_init_halt(const sigset_t *waited, bool restart) {
    static const int stop_signals[] = {SIGTERM, SIGHUP, SIGCONT};
    for (size_t i = 0; i < sizeof(stop_signals) / sizeof(stop_signals[0]); i++)
        (void)kill(-1, stop_signals[i]);
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
        if (kill(-1, 0) < 0 && errno == ESRCH)
            break;
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed_ms >= MANAGED_INIT_GRACE_MS)
            break;
        struct timespec slice = {.tv_nsec = MANAGED_INIT_POLL_NS};
        if (managed_init_poweroff_signal(sigtimedwait(waited, NULL, &slice)))
            restart = false;
    }
    (void)reboot(restart ? RB_AUTOBOOT : RB_POWER_OFF);
    _exit(0);
}

_Noreturn void managed_init_main(void) {
    (void)prctl(PR_SET_NAME, MANAGED_INIT_NAME, 0, 0, 0);
    sigset_t waited;
    managed_init_signals(&waited);
    for (;;) {
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
        int sig = sigwaitinfo(&waited, NULL);
        if (sig == SIGINT || sig == SIGRTMIN + 5)
            managed_init_halt(&waited, true);
        if (managed_init_poweroff_signal(sig))
            managed_init_halt(&waited, false);
    }
}

static int container_init_entry(void *arg) {
    init_child_args_t *a = arg;

    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);
    signal(SIGPIPE, SIG_DFL);

    if (a->hostname && a->hostname[0] != '\0')
        (void)sethostname(a->hostname, strlen(a->hostname));

    int rc = container_enter(false);
    if (rc == 0 && (umount2("/run", MNT_DETACH) < 0 || chdir(GUEST_ROOT) < 0 ||
                    mount(".", "/", NULL, MS_MOVE, NULL) < 0 || chroot(".") < 0 || chdir("/") < 0))
        rc = -errno;
    if (rc == 0)
        rc = container_mount_api();
    if (rc < 0)
        child_exit_error(a->err_wr, rc);

    uint8_t ok = 1;
    (void)write(a->ready_wr, &ok, 1);
    close(a->ready_wr);
    (void)read(a->go_rd, &ok, 1);
    close(a->go_rd);

    if (a->mode == SWIPTERMD_INIT_MANAGED) {
        static char *const managed_argv[] = {MANAGED_INIT_NAME, NULL};
        static char *const managed_envp[] = {"container=lxc", "PATH=" TRUSTED_PATH, NULL};
        (void)setsid();
        int null_fd = open("/dev/null", O_RDWR);
        if (null_fd < 0)
            child_exit_error(a->err_wr, -errno);
        redirect_stdio(null_fd);
        close_other_fds(&a->err_wr, 1);
        umask(0022);
        sigset_t waited;
        managed_init_signals(&waited);
        sigprocmask(SIG_SETMASK, &waited, NULL);
        execve(GUEST_TOOL_PATH, managed_argv, managed_envp);
        child_exit_error(a->err_wr, -errno);
    }

    (void)setsid();
    char pts_path[64];
    snprintf(pts_path, sizeof(pts_path), "/dev/pts/%d", a->pts_num);
    int console = open(pts_path, O_RDWR);
    if (console < 0 || ioctl(console, TIOCSCTTY, 0) < 0 || mount(pts_path, "/dev/console", NULL, MS_BIND, NULL) < 0)
        child_exit_error(a->err_wr, -errno);
    redirect_stdio(console);

    extern char **environ;
    environ = a->envp;
    if (setenv("container", "lxc", 1) < 0 || setenv("NOTIFY_SOCKET", GUEST_NOTIFY_SOCKET, 1) < 0 ||
        (g_daemon.gpu && setenv(GPU_VENUS_ENV_NAME, GPU_VENUS_ENV_VALUE, 0) < 0))
        child_exit_error(a->err_wr, -errno);
    umask(0022);
    execve(a->path, a->argv, environ);
    child_exit_error(a->err_wr, -errno);
}

static void close_pipe_end(int *fd) {
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

void ns_close_namespaces(void) {
    int *fds[] = {
        &g_daemon.container_pid_ns_fd, &g_daemon.container_mnt_ns_fd, &g_daemon.container_uts_ns_fd,
        &g_daemon.container_ipc_ns_fd, &g_daemon.container_cgroup_ns_fd,
    };
    for (size_t i = 0; i < sizeof(fds) / sizeof(fds[0]); i++) {
        if (*fds[i] >= 0) {
            close(*fds[i]);
            *fds[i] = -1;
        }
    }
}

static void prepare_managed_root(void) {
    int rc = fs_write_file_jailed("/etc/resolv.conf", (const uint8_t *)GUEST_DEFAULT_RESOLV_CONF,
                                  sizeof(GUEST_DEFAULT_RESOLV_CONF) - 1, 0644, false);
    if (rc < 0)
        boot_err_log("write /etc/resolv.conf", rc);
    struct stat st;
    if (g_daemon.hostname[0] != '\0') {
        if (fstatat(g_daemon.root_fd, "etc/hostname", &st, AT_SYMLINK_NOFOLLOW) < 0 || st.st_size == 0) {
            char line[sizeof(g_daemon.hostname) + 1];
            int line_len = snprintf(line, sizeof(line), "%s\n", g_daemon.hostname);
            rc = fs_write_file_jailed("/etc/hostname", (const uint8_t *)line, (size_t)line_len, 0644, false);
            if (rc < 0)
                boot_err_log("write /etc/hostname", rc);
        } else {
            apply_lxc_template("/etc/hostname");
        }
        apply_lxc_template("/etc/hosts");
    }
    if (fstatat(g_daemon.root_fd, "etc/hosts", &st, AT_SYMLINK_NOFOLLOW) < 0 || st.st_size == 0) {
        static const char hosts[] = "127.0.0.1 localhost\n::1 localhost ip6-localhost ip6-loopback\n";
        (void)fs_write_file_jailed("/etc/hosts", (const uint8_t *)hosts, sizeof(hosts) - 1, 0644, false);
    }
    ensure_hosts_entry();
    if (fstatat(g_daemon.root_fd, "etc/mtab", &st, AT_SYMLINK_NOFOLLOW) < 0) {
        int etc = fs_open_in_root("etc", O_PATH | O_DIRECTORY, 0);
        if (etc >= 0) {
            (void)symlinkat("/proc/self/mounts", etc, "mtab");
            close(etc);
        }
    }
}

void ns_handle_start_init(uint32_t req_id, const uint8_t *body, uint32_t len) {
    if (g_daemon.init_started || g_daemon.stopping) {
        ctrl_send_err_reply(SWIPTERMD_OP_START_INIT, req_id, g_daemon.stopping ? -ESHUTDOWN : -EBUSY);
        return;
    }

    swiptermd_buf_r_t r = {.data = body, .len = len};
    uint32_t mode = swiptermd_r_u32(&r);
    uint64_t console_token = swiptermd_r_u64(&r);
    uint8_t has_console = swiptermd_r_u8(&r);
    char path[4096];
    if (!swiptermd_r_str_copy(&r, path, sizeof(path)) ||
        (mode != SWIPTERMD_INIT_MANAGED && mode != SWIPTERMD_INIT_PROCESS)) {
        ctrl_send_err_reply(SWIPTERMD_OP_START_INIT, req_id, -EINVAL);
        return;
    }
    char **argv = strv_decode(&r);
    char **envp = argv ? strv_decode(&r) : NULL;
    if (!argv || !envp) {
        strv_free(argv);
        strv_free(envp);
        ctrl_send_err_reply(SWIPTERMD_OP_START_INIT, req_id, -EINVAL);
        return;
    }

    int pts_num = -1;
    int rc = 0;
    if (mode == SWIPTERMD_INIT_PROCESS) {
        rc = console_open(has_console != 0, console_token, &pts_num);
        if (rc == 0)
            rc = ttys_open();
        if (rc == 0 && g_daemon.hostname[0] != '\0') {
            apply_lxc_template("/etc/hostname");
            apply_lxc_template("/etc/hosts");
            ensure_hosts_entry();
        }
    } else {
        prepare_managed_root();
    }

    int ready_pipe[2] = {-1, -1};
    int go_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};
    size_t stack_size = 64 * 1024;
    uint8_t *stack = NULL;
    if (rc == 0 && (pipe2(ready_pipe, O_CLOEXEC) < 0 || pipe2(go_pipe, O_CLOEXEC) < 0 ||
                    pipe2(err_pipe, O_CLOEXEC) < 0))
        rc = -errno;
    if (rc == 0 && !(stack = malloc(stack_size)))
        rc = -ENOMEM;

    pid_t pid = -1;
    if (rc == 0) {
        init_child_args_t args = {
            .mode = mode,
            .hostname = g_daemon.hostname,
            .path = path,
            .argv = argv,
            .envp = envp,
            .pts_num = pts_num,
            .ready_wr = ready_pipe[1],
            .go_rd = go_pipe[0],
            .err_wr = err_pipe[1],
        };
        pid = clone(container_init_entry, stack + stack_size,
                    CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | SIGCHLD, &args);
        if (pid < 0)
            rc = -errno;
    }
    close_pipe_end(&ready_pipe[1]);
    close_pipe_end(&go_pipe[0]);
    close_pipe_end(&err_pipe[1]);

    int32_t child_err = 0;
    if (rc == 0) {
        uint8_t ready_byte = 0;
        if (read(ready_pipe[0], &ready_byte, 1) == 1) {
            g_daemon.container_pid_ns_fd = namespace_open(pid, "pid");
            g_daemon.container_mnt_ns_fd = namespace_open(pid, "mnt");
            g_daemon.container_uts_ns_fd = namespace_open(pid, "uts");
            g_daemon.container_ipc_ns_fd = namespace_open(pid, "ipc");
            g_daemon.container_cgroup_ns_fd = namespace_open(pid, "cgroup");
        }
        uint8_t go = 1;
        (void)write(go_pipe[1], &go, 1);
        if (read(err_pipe[0], &child_err, sizeof(child_err)) == (ssize_t)sizeof(child_err) && child_err < 0) {
            rc = child_err;
        } else if (g_daemon.container_pid_ns_fd < 0 || g_daemon.container_mnt_ns_fd < 0 ||
                   g_daemon.container_uts_ns_fd < 0 || g_daemon.container_ipc_ns_fd < 0 ||
                   g_daemon.container_cgroup_ns_fd < 0) {
            rc = -ESRCH;
            kill(pid, SIGKILL);
        }
        if (rc < 0) {
            int st;
            (void)waitpid(pid, &st, 0);
            ns_close_namespaces();
        }
    }
    close_pipe_end(&ready_pipe[0]);
    close_pipe_end(&go_pipe[1]);
    close_pipe_end(&err_pipe[0]);
    free(stack);
    strv_free(argv);
    strv_free(envp);

    if (rc < 0) {
        console_close_all();
        ttys_close();
        ctrl_send_err_reply(SWIPTERMD_OP_START_INIT, req_id, rc);
        return;
    }

    g_daemon.init_mode = mode;
    g_daemon.container_init_pid = pid;
    g_daemon.init_started = true;
    ctrl_send_err_reply(SWIPTERMD_OP_START_INIT, req_id, 0);
    if (mode == SWIPTERMD_INIT_MANAGED) {
        ready_announce(SWIPTERMD_READY_LOGIN);
        ready_announce(SWIPTERMD_READY_SYSTEM);
        return;
    }
    if (!ns_init_is_systemd() && login_available())
        ready_announce(SWIPTERMD_READY_LOGIN);
    rc = readiness_start();
    if (rc < 0)
        boot_err_log("arm the readiness probe", rc);
}

bool ns_init_executable(char *name, size_t cap) {
    if (g_daemon.container_init_pid <= 0)
        return false;
    char path[64];
    char exe[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/exe", (int)g_daemon.container_init_pid);
    ssize_t n = readlink(path, exe, sizeof(exe) - 1);
    if (n <= 0)
        return false;
    exe[n] = '\0';
    const char *base = strrchr(exe, '/');
    return base && snprintf(name, cap, "%s", base + 1) < (int)cap;
}

bool ns_init_is_systemd(void) {
    char name[NAME_MAX + 1];
    return ns_init_executable(name, sizeof(name)) && strcmp(name, "systemd") == 0;
}

void ns_init_exited(int status) {
    g_daemon.container_init_pid = -1;
    readiness_stop();
    ns_close_namespaces();
    console_drain();
    ttys_close();

    uint32_t reason = SWIPTERMD_HALT_REASON_EXITED;
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGHUP)
        reason = SWIPTERMD_HALT_REASON_REBOOT;
    else if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
        reason = SWIPTERMD_HALT_REASON_POWEROFF;
    uint8_t msg[SWIPTERMD_HALT_NOTIFY_SIZE];
    swiptermd_enc_halt_notify(msg, reason, status);
    ctrl_send_frame(SWIPTERMD_OP_HALT, 0, msg, sizeof(msg));
    shutdown_begin(false);
    shutdown_check_done();
}

static void on_stop_helper_exit(int status) {
    pid_t pid = g_daemon.container_init_pid;
    if (pid > 0 && !(WIFEXITED(status) && WEXITSTATUS(status) == 0))
        kill(pid, g_stop_reboot ? SIGINT : SIGPWR);
}

void ns_request_stop(bool reboot) {
    pid_t pid = g_daemon.container_init_pid;
    if (pid <= 0)
        return;
    if (g_daemon.init_mode == SWIPTERMD_INIT_MANAGED || notify_signals_level() >= 2 || ns_init_is_systemd()) {
        kill(pid, reboot ? SIGRTMIN + 5 : SIGRTMIN + 3);
        return;
    }
    g_stop_reboot = reboot;
    char *const argv[] = {reboot ? "reboot" : "poweroff", NULL};
    if (proc_run_in_container(argv, on_stop_helper_exit) < 0)
        kill(pid, reboot ? SIGINT : SIGPWR);
}

void ns_kill_container(void) {
    pid_t pid = g_daemon.container_init_pid;
    if (pid > 0) {
        kill(pid, SIGKILL);
        int status;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
        g_daemon.container_init_pid = -1;
    }
    readiness_stop();
    ns_close_namespaces();
    console_close_all();
    ttys_close();
}
