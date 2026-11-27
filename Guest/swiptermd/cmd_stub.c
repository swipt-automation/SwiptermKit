#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "guest_ipc.h"
#include "wire_format.h"

extern char **environ;

static int resolve_sock_path(char *out, size_t cap) {
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0)
        return -1;
    exe[n] = '\0';
    char *slash = strrchr(exe, '/');
    if (!slash)
        return -1;
    *slash = '\0';
    return snprintf(out, cap, "%s/" HOST_CMD_SOCKET_NAME, exe) < (int)cap ? 0 : -1;
}

static size_t strv_wire_size(char *const *vec, uint32_t count) {
    size_t size = 4;
    for (uint32_t i = 0; i < count; i++)
        size += 4 + strlen(vec[i]);
    return size;
}

static int send_stdio_fds(int sock, uint32_t payload_len) {
    uint8_t hdr[4];
    swiptermd_put_be32(hdr, payload_len);

    int fds[3];
    int opened[3] = {-1, -1, -1};
    for (int i = 0; i < 3; i++) {
        if (fcntl(i, F_GETFD) >= 0) {
            fds[i] = i;
        } else {
            int nfd = open("/dev/null", i == 0 ? O_RDONLY : O_WRONLY);
            if (nfd < 0)
                return -1;
            fds[i] = nfd;
            opened[i] = nfd;
        }
    }

    struct iovec iov = {.iov_base = hdr, .iov_len = sizeof(hdr)};
    union {
        struct cmsghdr align;
        uint8_t buf[CMSG_SPACE(sizeof(fds))];
    } cmsg_u;
    memset(&cmsg_u, 0, sizeof(cmsg_u));

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_u.buf;
    msg.msg_controllen = sizeof(cmsg_u.buf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fds));
    memcpy(CMSG_DATA(cmsg), fds, sizeof(fds));

    ssize_t sent;
    do {
        sent = sendmsg(sock, &msg, MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);

    for (int i = 0; i < 3; i++) {
        if (opened[i] >= 0)
            close(opened[i]);
    }
    return sent == (ssize_t)sizeof(hdr) ? 0 : -1;
}

int main(int argc, char **argv) {
    const char *raw0 = (argc > 0 && argv[0]) ? argv[0] : "";
    const char *slash = strrchr(raw0, '/');
    const char *name = slash ? slash + 1 : raw0;
    if (name[0] != '\0')
        prctl(PR_SET_NAME, name, 0, 0, 0);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    signal(SIGPIPE, SIG_IGN);

    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (sfd < 0)
        _exit(127);

    char sock_path[PATH_MAX];
    if (resolve_sock_path(sock_path, sizeof(sock_path)) < 0)
        _exit(127);

    int sock = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (sock < 0)
        _exit(127);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(sock_path) >= sizeof(addr.sun_path))
        _exit(127);
    strcpy(addr.sun_path, sock_path);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        _exit(127);

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd)))
        strcpy(cwd, "/");

    uint32_t nargs = argc > 1 ? (uint32_t)(argc - 1) : 0;
    uint32_t nenv = 0;
    while (environ && environ[nenv])
        nenv++;
    size_t plen = 8 + strlen(name) + strlen(cwd) + strv_wire_size(argv + 1, nargs) + strv_wire_size(environ, nenv);
    uint8_t *payload = malloc(plen);
    if (!payload)
        _exit(127);
    swiptermd_buf_w_t w = {.data = payload, .cap = plen};
    swiptermd_w_str(&w, name);
    swiptermd_w_str(&w, cwd);
    swiptermd_w_strv(&w, (const char *const *)argv + 1, nargs);
    swiptermd_w_strv(&w, (const char *const *)environ, nenv);
    if (send_stdio_fds(sock, (uint32_t)w.len) < 0 || write_all_blocking(sock, payload, w.len) < 0)
        _exit(127);
    free(payload);

    struct pollfd pfds[2] = {
        {.fd = sock, .events = POLLIN},
        {.fd = sfd, .events = POLLIN},
    };

    for (;;) {
        int r = poll(pfds, 2, -1);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            _exit(127);
        }

        if (pfds[1].revents & POLLIN) {
            struct signalfd_siginfo si;
            if (read(sfd, &si, sizeof(si)) == (ssize_t)sizeof(si)) {
                stub_ctl_msg_t msg = {
                    .type = STUB_MSG_CANCEL,
                    .value = (int32_t)si.ssi_signo,
                };
                (void)write_all_blocking(sock, &msg, sizeof(msg));
                close(sock);
                close(sfd);

                sigset_t unblock;
                sigemptyset(&unblock);
                sigaddset(&unblock, (int)si.ssi_signo);
                signal((int)si.ssi_signo, SIG_DFL);
                sigprocmask(SIG_UNBLOCK, &unblock, NULL);
                raise((int)si.ssi_signo);
                _exit(128 + (int)si.ssi_signo);
            }
        }

        if (pfds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            stub_ctl_msg_t msg;
            if (read_all_blocking(sock, &msg, sizeof(msg)) < 0)
                _exit(127);
            if (msg.type == STUB_MSG_EXIT)
                _exit(msg.value & 0xff);
        }
    }
}
