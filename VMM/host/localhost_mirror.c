#define _DARWIN_C_SOURCE

#include "../../Guest/swiptermd/wire_format.h"
#include "host_internal.h"
#include "rpc_codec.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct mirror_listener {
    uint8_t family;
    uint16_t port;
    int fd;
} mirror_listener_t;

struct swipterm_mirror {
    virtio_dev_t *vsock;
    _Atomic uint64_t session_nonce;
    _Atomic uint64_t conn_seq;
    _Atomic bool stop;

    pthread_mutex_t lock;
    pthread_cond_t relay_cond;
    size_t active_relays;
    mirror_listener_t *listeners;
    size_t count;
    size_t cap;

    int wake_pipe[2];
    int stop_pipe[2];
    pthread_t thread;
    bool thread_started;
};

typedef struct relay_args {
    swipterm_mirror_t *mirror;
    int host_fd;
    int vsock_fd;
} relay_args_t;

static void set_nosigpipe(int fd) {
    int one = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
}

static void *mirror_relay_thread(void *arg) {
    relay_args_t *ra = arg;
    swipterm_mirror_t *m = ra->mirror;
    int a = ra->host_fd;
    int b = ra->vsock_fd;
    free(ra);

    set_nosigpipe(a);
    set_nosigpipe(b);

    bool a_eof = false;
    bool b_eof = false;
    uint8_t buf[32768];

    while ((!a_eof || !b_eof) && !atomic_load(&m->stop)) {
        struct pollfd pfds[3] = {
            {.fd = a_eof ? -1 : a, .events = POLLIN},
            {.fd = b_eof ? -1 : b, .events = POLLIN},
            {.fd = m->stop_pipe[0], .events = POLLIN},
        };
        int ready = poll(pfds, 3, 1000);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (ready == 0) {
            continue;
        }
        if (pfds[2].revents != 0 || atomic_load(&m->stop)) {
            break;
        }
        if (!a_eof && (pfds[0].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t n = read(a, buf, sizeof(buf));
            if (n > 0) {
                if (!rpc_write_all(b, buf, (size_t)n)) {
                    break;
                }
            } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                a_eof = true;
                (void)shutdown(b, SHUT_WR);
            }
        }
        if (!b_eof && (pfds[1].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t n = read(b, buf, sizeof(buf));
            if (n > 0) {
                if (!rpc_write_all(a, buf, (size_t)n)) {
                    break;
                }
            } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                b_eof = true;
                (void)shutdown(a, SHUT_WR);
            }
        }
    }

    close(a);
    close(b);

    pthread_mutex_lock(&m->lock);
    m->active_relays--;
    if (m->active_relays == 0) {
        pthread_cond_broadcast(&m->relay_cond);
    }
    pthread_mutex_unlock(&m->lock);
    return NULL;
}

static void mirror_handle_accept(swipterm_mirror_t *m, uint8_t family, uint16_t port, int client_fd) {
    if (atomic_load(&m->stop)) {
        close(client_fd);
        return;
    }
    set_nosigpipe(client_fd);
    int vsock_fd = virtio_vsock_connect_host(m->vsock, SWIPTERMD_DATA_PORT);
    if (vsock_fd < 0) {
        close(client_fd);
        return;
    }
    set_nosigpipe(vsock_fd);

    swiptermd_attach_hdr_t hdr = {
        .session_nonce = atomic_load(&m->session_nonce),
        .role = SWIPTERMD_STREAM_TCP_MIRROR,
        .resource_id = swiptermd_mirror_pack_resource_id(family, port),
        .token = atomic_fetch_add(&m->conn_seq, 1) + 1,
    };
    uint8_t raw[SWIPTERMD_ATTACH_HEADER_SIZE];
    swiptermd_attach_hdr_encode(raw, &hdr);
    if (!rpc_write_all(vsock_fd, raw, sizeof(raw))) {
        close(vsock_fd);
        close(client_fd);
        return;
    }

    relay_args_t *ra = malloc(sizeof(*ra));
    if (!ra) {
        close(vsock_fd);
        close(client_fd);
        return;
    }
    ra->mirror = m;
    ra->host_fd = client_fd;
    ra->vsock_fd = vsock_fd;

    pthread_mutex_lock(&m->lock);
    if (atomic_load(&m->stop)) {
        pthread_mutex_unlock(&m->lock);
        free(ra);
        close(vsock_fd);
        close(client_fd);
        return;
    }
    m->active_relays++;
    pthread_mutex_unlock(&m->lock);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t tid;
    if (pthread_create(&tid, &attr, mirror_relay_thread, ra) != 0) {
        pthread_mutex_lock(&m->lock);
        m->active_relays--;
        if (m->active_relays == 0) {
            pthread_cond_broadcast(&m->relay_cond);
        }
        pthread_mutex_unlock(&m->lock);
        free(ra);
        close(vsock_fd);
        close(client_fd);
    }
    pthread_attr_destroy(&attr);
}

static void mirror_wake(swipterm_mirror_t *m) {
    uint8_t b = 1;
    (void)write(m->wake_pipe[1], &b, 1);
}

static void *mirror_accept_thread(void *arg) {
    swipterm_mirror_t *m = arg;

    while (!atomic_load(&m->stop)) {
        pthread_mutex_lock(&m->lock);
        size_t count = m->count;
        struct pollfd *pfds = malloc((count + 1) * sizeof(*pfds));
        mirror_listener_t *snap = count ? malloc(count * sizeof(*snap)) : NULL;
        if (!pfds || (count && !snap)) {
            free(pfds);
            free(snap);
            pthread_mutex_unlock(&m->lock);
            break;
        }
        pfds[0] = (struct pollfd){.fd = m->wake_pipe[0], .events = POLLIN};
        for (size_t i = 0; i < count; i++) {
            snap[i] = m->listeners[i];
            pfds[i + 1] = (struct pollfd){.fd = snap[i].fd, .events = POLLIN};
        }
        pthread_mutex_unlock(&m->lock);

        int ready = poll(pfds, (nfds_t)(count + 1), -1);
        if (ready > 0) {
            if (pfds[0].revents & POLLIN) {
                uint8_t drain[64];
                while (read(m->wake_pipe[0], drain, sizeof(drain)) > 0) {}
            }
            for (size_t i = 0; i < count; i++) {
                if (pfds[i + 1].revents & POLLIN) {
                    for (;;) {
                        int cfd = accept(snap[i].fd, NULL, NULL);
                        if (cfd < 0) {
                            break;
                        }
                        mirror_handle_accept(m, snap[i].family, snap[i].port, cfd);
                    }
                }
            }
        }
        free(pfds);
        free(snap);
    }
    return NULL;
}

static int open_loopback_listener(uint8_t family, uint16_t port) {
    int domain = (family == SWIPTERMD_MIRROR_AF_INET6) ? AF_INET6 : AF_INET;
    int fd = socket(domain, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    int one = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    set_nosigpipe(fd);
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);

    if (domain == AF_INET6) {
        (void)setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one));
        struct sockaddr_in6 addr6;
        memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(port);
        addr6.sin6_addr = in6addr_loopback;
        if (bind(fd, (struct sockaddr *)&addr6, sizeof(addr6)) != 0 || listen(fd, 64) != 0) {
            close(fd);
            return -1;
        }
    } else {
        struct sockaddr_in addr4;
        memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(port);
        addr4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(fd, (struct sockaddr *)&addr4, sizeof(addr4)) != 0 || listen(fd, 64) != 0) {
            close(fd);
            return -1;
        }
    }
    return fd;
}

static void mirror_add_locked(swipterm_mirror_t *m, uint8_t family, uint16_t port) {
    if (port == 0) {
        return;
    }
    for (size_t i = 0; i < m->count; i++) {
        if (m->listeners[i].family == family && m->listeners[i].port == port) {
            return;
        }
    }
    int fd = open_loopback_listener(family, port);
    if (fd < 0) {
        return;
    }
    if (m->count == m->cap) {
        size_t next_cap = m->cap ? m->cap * 2 : 16;
        mirror_listener_t *next = realloc(m->listeners, next_cap * sizeof(*next));
        if (!next) {
            close(fd);
            return;
        }
        m->listeners = next;
        m->cap = next_cap;
    }
    m->listeners[m->count++] = (mirror_listener_t){
        .family = family,
        .port = port,
        .fd = fd,
    };
}

static void mirror_remove_locked(swipterm_mirror_t *m, uint8_t family, uint16_t port) {
    for (size_t i = 0; i < m->count; i++) {
        if (m->listeners[i].family == family && m->listeners[i].port == port) {
            close(m->listeners[i].fd);
            if (i + 1 < m->count) {
                m->listeners[i] = m->listeners[m->count - 1];
            }
            m->count--;
            return;
        }
    }
}

swipterm_mirror_t *swipterm_mirror_create(virtio_dev_t *vsock, uint64_t session_nonce) {
    swipterm_mirror_t *m = calloc(1, sizeof(*m));
    if (!m) {
        return NULL;
    }
    m->vsock = vsock;
    atomic_init(&m->session_nonce, session_nonce);
    atomic_init(&m->conn_seq, 0);
    atomic_init(&m->stop, false);
    pthread_mutex_init(&m->lock, NULL);
    pthread_cond_init(&m->relay_cond, NULL);
    m->wake_pipe[0] = m->wake_pipe[1] = -1;
    m->stop_pipe[0] = m->stop_pipe[1] = -1;

    if (pipe(m->wake_pipe) != 0 || pipe(m->stop_pipe) != 0) {
        if (m->wake_pipe[0] >= 0) close(m->wake_pipe[0]);
        if (m->wake_pipe[1] >= 0) close(m->wake_pipe[1]);
        if (m->stop_pipe[0] >= 0) close(m->stop_pipe[0]);
        if (m->stop_pipe[1] >= 0) close(m->stop_pipe[1]);
        pthread_cond_destroy(&m->relay_cond);
        pthread_mutex_destroy(&m->lock);
        free(m);
        return NULL;
    }
    for (int i = 0; i < 2; i++) {
        int fl = fcntl(m->wake_pipe[i], F_GETFL, 0);
        if (fl >= 0) {
            (void)fcntl(m->wake_pipe[i], F_SETFL, fl | O_NONBLOCK);
        }
        (void)fcntl(m->wake_pipe[i], F_SETFD, FD_CLOEXEC);
        (void)fcntl(m->stop_pipe[i], F_SETFD, FD_CLOEXEC);
    }

    if (pthread_create(&m->thread, NULL, mirror_accept_thread, m) == 0) {
        m->thread_started = true;
    }
    return m;
}

void swipterm_mirror_set_nonce(swipterm_mirror_t *mirror, uint64_t session_nonce) {
    if (mirror) {
        atomic_store(&mirror->session_nonce, session_nonce);
    }
}

void swipterm_mirror_apply_updates(swipterm_mirror_t *mirror,
                                   const swiptermd_mirror_entry_t *entries,
                                   size_t count) {
    if (!mirror || !entries || count == 0) {
        return;
    }
    pthread_mutex_lock(&mirror->lock);
    for (size_t i = 0; i < count; i++) {
        uint8_t fam = entries[i].family;
        uint16_t port = entries[i].port;
        if (entries[i].action == SWIPTERMD_MIRROR_ADDED) {
            mirror_add_locked(mirror, fam, port);
        } else if (entries[i].action == SWIPTERMD_MIRROR_REMOVED) {
            mirror_remove_locked(mirror, fam, port);
        }
    }
    pthread_mutex_unlock(&mirror->lock);
    mirror_wake(mirror);
}

void swipterm_mirror_destroy(swipterm_mirror_t *mirror) {
    if (!mirror) {
        return;
    }
    atomic_store(&mirror->stop, true);
    if (mirror->stop_pipe[1] >= 0) {
        close(mirror->stop_pipe[1]);
        mirror->stop_pipe[1] = -1;
    }
    mirror_wake(mirror);
    if (mirror->thread_started) {
        pthread_join(mirror->thread, NULL);
    }
    pthread_mutex_lock(&mirror->lock);
    while (mirror->active_relays > 0) {
        pthread_cond_wait(&mirror->relay_cond, &mirror->lock);
    }
    for (size_t i = 0; i < mirror->count; i++) {
        close(mirror->listeners[i].fd);
    }
    free(mirror->listeners);
    pthread_mutex_unlock(&mirror->lock);
    pthread_cond_destroy(&mirror->relay_cond);
    pthread_mutex_destroy(&mirror->lock);
    close(mirror->wake_pipe[0]);
    close(mirror->wake_pipe[1]);
    if (mirror->stop_pipe[0] >= 0) {
        close(mirror->stop_pipe[0]);
    }
    free(mirror);
}
