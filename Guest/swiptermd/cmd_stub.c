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

extern char **environ;

enum {
    STUB_MSG_EXIT = 1,
    STUB_MSG_CANCEL = 2,
};

struct stub_ctl_msg {
    uint32_t type;
    int32_t value;
};

static int write_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static int read_all(int fd, void *buf, size_t len) {
    uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static int append_u32(uint8_t **buf, size_t *len, size_t *cap, uint32_t v) {
    if (*len + 4 > *cap) {
        size_t ncap = *cap ? *cap * 2 : 4096;
        while (ncap < *len + 4)
            ncap *= 2;
        uint8_t *nb = realloc(*buf, ncap);
        if (!nb)
            return -1;
        *buf = nb;
        *cap = ncap;
    }
    (*buf)[*len + 0] = (uint8_t)(v >> 24);
    (*buf)[*len + 1] = (uint8_t)(v >> 16);
    (*buf)[*len + 2] = (uint8_t)(v >> 8);
    (*buf)[*len + 3] = (uint8_t)v;
    *len += 4;
    return 0;
}

static int append_str(uint8_t **buf, size_t *len, size_t *cap, const char *s) {
    uint32_t slen = s ? (uint32_t)strlen(s) : 0;
    if (append_u32(buf, len, cap, slen) < 0)
        return -1;
    if (slen == 0)
        return 0;
    if (*len + slen > *cap) {
        size_t ncap = *cap ? *cap * 2 : 4096;
        while (ncap < *len + slen)
            ncap *= 2;
        uint8_t *nb = realloc(*buf, ncap);
        if (!nb)
            return -1;
        *buf = nb;
        *cap = ncap;
    }
    memcpy(*buf + *len, s, slen);
    *len += slen;
    return 0;
}

static void resolve_sock_path(char *out, size_t cap) {
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        exe[n] = '\0';
        char *slash = strrchr(exe, '/');
        if (slash) {
            *slash = '\0';
            if ((size_t)snprintf(out, cap, "%s/.sock", exe) < cap)
                return;
        }
    }
    snprintf(out, cap, "/run/host-bin/.sock");
}

static int send_stdio_fds(int sock, uint32_t payload_len) {
    uint8_t hdr[4];
    hdr[0] = (uint8_t)(payload_len >> 24);
    hdr[1] = (uint8_t)(payload_len >> 16);
    hdr[2] = (uint8_t)(payload_len >> 8);
    hdr[3] = (uint8_t)payload_len;

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
    resolve_sock_path(sock_path, sizeof(sock_path));

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

    uint8_t *payload = NULL;
    size_t plen = 0;
    size_t pcap = 0;

    if (append_str(&payload, &plen, &pcap, name) < 0 ||
        append_str(&payload, &plen, &pcap, cwd) < 0)
        _exit(127);

    uint32_t nargs = argc > 1 ? (uint32_t)(argc - 1) : 0;
    if (append_u32(&payload, &plen, &pcap, nargs) < 0)
        _exit(127);
    for (uint32_t i = 0; i < nargs; i++) {
        if (append_str(&payload, &plen, &pcap, argv[i + 1]) < 0)
            _exit(127);
    }

    uint32_t nenv = 0;
    if (environ) {
        while (environ[nenv])
            nenv++;
    }
    if (append_u32(&payload, &plen, &pcap, nenv) < 0)
        _exit(127);
    for (uint32_t i = 0; i < nenv; i++) {
        if (append_str(&payload, &plen, &pcap, environ[i]) < 0)
            _exit(127);
    }

    if (send_stdio_fds(sock, (uint32_t)plen) < 0 || write_all(sock, payload, plen) < 0)
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
                struct stub_ctl_msg msg = {
                    .type = STUB_MSG_CANCEL,
                    .value = (int32_t)si.ssi_signo,
                };
                (void)write_all(sock, &msg, sizeof(msg));
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
            struct stub_ctl_msg msg;
            if (read_all(sock, &msg, sizeof(msg)) < 0)
                _exit(127);
            if (msg.type == STUB_MSG_EXIT)
                _exit(msg.value & 0xff);
        }
    }
}
