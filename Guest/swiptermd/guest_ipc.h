#ifndef SWIPTERMD_GUEST_IPC_H
#define SWIPTERMD_GUEST_IPC_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#define GUEST_RUN_HOST "/run/host"
#define GUEST_CLIPBOARD_SOCKET GUEST_RUN_HOST "/clipboard.sock"
#define HOST_CMD_SOCKET_NAME ".sock"

enum {
    CLIP_SOCK_LIST = 1,
    CLIP_SOCK_READ = 2,
    CLIP_SOCK_WRITE = 3,
    CLIP_MAX_MIME_LEN = 256,
};

enum {
    STUB_MSG_EXIT = 1,
    STUB_MSG_CANCEL = 2,
    STUB_MSG_WINCH = 3,
};

typedef struct stub_ctl_msg {
    uint32_t type;
    int32_t value;
} stub_ctl_msg_t;

static inline int write_all_blocking(int fd, const void *buf, size_t len) {
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

static inline int read_all_blocking(int fd, void *buf, size_t len) {
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

enum {
    CLIP_SOCK_HEADER_SIZE = 8,
};

#endif
