#include "swiptermd.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static ev_node_t g_notify_ev;
static int g_notify_fd = -1;
static int g_signals_level;

static void handle_init_message(char *message) {
    char *save = NULL;
    for (char *line = strtok_r(message, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        int level = 0;
        if (strcmp(line, "READY=1") == 0)
            ready_announce(SWIPTERMD_READY_SYSTEM);
        else if (sscanf(line, "X_SYSTEMD_SIGNALS_LEVEL=%d", &level) == 1 && level > 0)
            g_signals_level = level;
    }
}

static void on_notify(ev_node_t *node, uint32_t events) {
    (void)events;
    for (;;) {
        char data[4096];
        union {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(struct ucred))];
        } control;
        struct iovec iov = {.iov_base = data, .iov_len = sizeof(data) - 1};
        struct msghdr msg = {
            .msg_iov = &iov,
            .msg_iovlen = 1,
            .msg_control = control.buf,
            .msg_controllen = sizeof(control.buf),
        };
        ssize_t n = recvmsg(node->fd, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))
            continue;
        struct cmsghdr *header = CMSG_FIRSTHDR(&msg);
        if (!header || header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_CREDENTIALS ||
            header->cmsg_len < CMSG_LEN(sizeof(struct ucred)))
            continue;
        struct ucred cred;
        memcpy(&cred, CMSG_DATA(header), sizeof(cred));
        data[n] = '\0';
        if (cred.pid > 0 && cred.pid == g_daemon.container_init_pid)
            handle_init_message(data);
        else if (cred.uid == 0 && strcmp(data, "X_SWIPTERM_SESSIONS=1") == 0)
            sessions_register(cred.pid);
    }
}

int notify_setup(void) {
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    if (snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", GUEST_ROOT GUEST_NOTIFY_SOCKET) >=
        (int)sizeof(addr.sun_path))
        return -ENAMETOOLONG;
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -errno;
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &yes, sizeof(yes)) < 0 ||
        bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        int err = -errno;
        close(fd);
        return err;
    }
    int rc = ev_add(&g_notify_ev, fd, EPOLLIN, on_notify, NULL);
    if (rc < 0) {
        close(fd);
        return rc;
    }
    g_notify_fd = fd;
    return 0;
}

void notify_close(void) {
    if (g_notify_fd < 0)
        return;
    ev_del(&g_notify_ev);
    close(g_notify_fd);
    g_notify_fd = -1;
}

int notify_signals_level(void) {
    return g_signals_level;
}
