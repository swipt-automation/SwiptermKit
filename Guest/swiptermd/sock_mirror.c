#include "swiptermd.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#define MIRROR_MAX_LISTENERS 256
#define MIRROR_BUF_CAP 32768

typedef struct listener_key {
    uint8_t family;
    uint16_t port;
    uint8_t addr[16];
} listener_key_t;

typedef struct tcp_proxy {
    int vsock_fd;
    int tcp_fd;
    ev_node_t vsock_ev;
    ev_node_t tcp_ev;
    bool connecting;
    bool vsock_eof;
    bool tcp_eof;

    uint8_t v2t_buf[MIRROR_BUF_CAP];
    size_t v2t_off;
    size_t v2t_len;

    uint8_t t2v_buf[MIRROR_BUF_CAP];
    size_t t2v_off;
    size_t t2v_len;
} tcp_proxy_t;

static int g_diag_nl_fd = -1;
static int g_scan_timer_fd = -1;
static ev_node_t g_scan_timer_ev;
static listener_key_t g_prev_listeners[MIRROR_MAX_LISTENERS];
static size_t g_prev_count;

static bool listener_eq(const listener_key_t *a, const listener_key_t *b) {
    return a->family == b->family && a->port == b->port && memcmp(a->addr, b->addr, 16) == 0;
}

static bool is_mirrorable_addr(uint8_t family, const uint8_t addr[16]) {
    if (family == SWIPTERMD_MIRROR_AF_INET) {
        if (addr[0] == 0 && addr[1] == 0 && addr[2] == 0 && addr[3] == 0)
            return true;
        if (addr[0] == 127)
            return true;
        return false;
    }
    if (family == SWIPTERMD_MIRROR_AF_INET6) {
        static const uint8_t any6[16] = {0};
        static const uint8_t loop6[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        return memcmp(addr, any6, 16) == 0 || memcmp(addr, loop6, 16) == 0;
    }
    return false;
}

static void scan_family(int af, uint8_t wire_family, listener_key_t *out, size_t *count) {
    if (g_diag_nl_fd < 0)
        return;

    struct {
        struct nlmsghdr nlh;
        struct inet_diag_req_v2 req;
    } msg;
    memset(&msg, 0, sizeof(msg));
    msg.nlh.nlmsg_len = sizeof(msg);
    msg.nlh.nlmsg_type = SOCK_DIAG_BY_FAMILY;
    msg.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    msg.req.sdiag_family = (uint8_t)af;
    msg.req.sdiag_protocol = IPPROTO_TCP;
    msg.req.idiag_states = 1U << TCP_LISTEN;

    if (send(g_diag_nl_fd, &msg, sizeof(msg), 0) < 0)
        return;

    uint8_t buf[16384];
    for (;;) {
        ssize_t n = recv(g_diag_nl_fd, buf, sizeof(buf), 0);
        if (n <= 0)
            return;

        for (struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
             NLMSG_OK(nlh, (unsigned int)n);
             nlh = NLMSG_NEXT(nlh, n)) {
            if (nlh->nlmsg_type == NLMSG_DONE)
                return;
            if (nlh->nlmsg_type == NLMSG_ERROR)
                return;
            if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(struct inet_diag_msg)))
                continue;

            const struct inet_diag_msg *dm = NLMSG_DATA(nlh);
            uint16_t port = ntohs(dm->id.idiag_sport);
            if (port == 0)
                continue;

            listener_key_t key;
            memset(&key, 0, sizeof(key));
            key.family = wire_family;
            key.port = port;
            if (af == AF_INET)
                memcpy(key.addr, dm->id.idiag_src, 4);
            else
                memcpy(key.addr, dm->id.idiag_src, 16);

            if (!is_mirrorable_addr(wire_family, key.addr))
                continue;

            bool dup = false;
            for (size_t i = 0; i < *count; i++) {
                if (listener_eq(&out[i], &key)) {
                    dup = true;
                    break;
                }
            }
            if (!dup && *count < MIRROR_MAX_LISTENERS)
                out[(*count)++] = key;
        }
    }
}

void sock_mirror_scan_now(void) {
    if (g_diag_nl_fd < 0)
        return;

    listener_key_t cur[MIRROR_MAX_LISTENERS];
    size_t cur_count = 0;
    scan_family(AF_INET, SWIPTERMD_MIRROR_AF_INET, cur, &cur_count);
    scan_family(AF_INET6, SWIPTERMD_MIRROR_AF_INET6, cur, &cur_count);

    swiptermd_mirror_entry_t diffs[MIRROR_MAX_LISTENERS * 2];
    uint32_t diff_count = 0;

    for (size_t i = 0; i < cur_count; i++) {
        bool existed = false;
        for (size_t j = 0; j < g_prev_count; j++) {
            if (listener_eq(&cur[i], &g_prev_listeners[j])) {
                existed = true;
                break;
            }
        }
        if (!existed) {
            swiptermd_mirror_entry_t e = {
                .family = cur[i].family,
                .action = SWIPTERMD_MIRROR_ADDED,
                .port = cur[i].port,
            };
            memcpy(e.addr, cur[i].addr, 16);
            diffs[diff_count++] = e;
        }
    }

    for (size_t i = 0; i < g_prev_count; i++) {
        bool still = false;
        for (size_t j = 0; j < cur_count; j++) {
            if (listener_eq(&g_prev_listeners[i], &cur[j])) {
                still = true;
                break;
            }
        }
        if (!still) {
            swiptermd_mirror_entry_t e = {
                .family = g_prev_listeners[i].family,
                .action = SWIPTERMD_MIRROR_REMOVED,
                .port = g_prev_listeners[i].port,
            };
            memcpy(e.addr, g_prev_listeners[i].addr, 16);
            diffs[diff_count++] = e;
        }
    }

    memcpy(g_prev_listeners, cur, cur_count * sizeof(cur[0]));
    g_prev_count = cur_count;

    if (diff_count == 0)
        return;

    size_t wire_len = 4 + (size_t)diff_count * SWIPTERMD_MIRROR_ENTRY_WIRE_SIZE;
    uint8_t *wire = malloc(wire_len);
    if (!wire)
        return;
    swiptermd_buf_w_t w = swiptermd_buf_w_init(wire, wire_len);
    swiptermd_w_u32(&w, diff_count);
    for (uint32_t i = 0; i < diff_count; i++)
        swiptermd_mirror_entry_encode(&w, &diffs[i]);
    ctrl_send_frame(SWIPTERMD_OP_MIRROR_UPDATE, 0, wire, (uint32_t)w.len);
    free(wire);
}

static void on_scan_timer(ev_node_t *node, uint32_t events) {
    (void)events;
    uint64_t expirations;
    while (read(node->fd, &expirations, sizeof(expirations)) > 0) {}
    sock_mirror_scan_now();
}

void sock_mirror_init(void) {
    g_diag_nl_fd = guest_socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_SOCK_DIAG);
    if (g_diag_nl_fd >= 0) {
        struct timeval tv = {.tv_sec = 0, .tv_usec = 100000};
        setsockopt(g_diag_nl_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    g_scan_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (g_scan_timer_fd >= 0) {
        struct itimerspec its = {
            .it_interval = {.tv_sec = 0, .tv_nsec = 500000000L},
            .it_value = {.tv_sec = 0, .tv_nsec = 500000000L},
        };
        timerfd_settime(g_scan_timer_fd, 0, &its, NULL);
        ev_add(&g_scan_timer_ev, g_scan_timer_fd, EPOLLIN, on_scan_timer, NULL);
    }
}

static void tcp_proxy_destroy(tcp_proxy_t *p) {
    ev_del(&p->vsock_ev);
    ev_del(&p->tcp_ev);
    if (p->vsock_fd >= 0)
        close(p->vsock_fd);
    if (p->tcp_fd >= 0)
        close(p->tcp_fd);
    free(p);
}

static void tcp_proxy_update_events(tcp_proxy_t *p) {
    if (p->connecting) {
        ev_mod(&p->vsock_ev, 0);
        ev_mod(&p->tcp_ev, EPOLLOUT);
        return;
    }
    uint32_t vev = 0;
    if (!p->vsock_eof && p->v2t_len == 0)
        vev |= EPOLLIN;
    if (p->t2v_off < p->t2v_len)
        vev |= EPOLLOUT;

    uint32_t tev = 0;
    if (!p->tcp_eof && p->t2v_len == 0)
        tev |= EPOLLIN;
    if (p->v2t_off < p->v2t_len)
        tev |= EPOLLOUT;

    ev_mod(&p->vsock_ev, vev);
    ev_mod(&p->tcp_ev, tev);
}

static void on_tcp_proxy_event(ev_node_t *node, uint32_t events) {
    tcp_proxy_t *p = node->ctx;

    if (p->connecting && node == &p->tcp_ev) {
        int err = 0;
        socklen_t elen = sizeof(err);
        if (getsockopt(p->tcp_fd, SOL_SOCKET, SO_ERROR, &err, &elen) < 0 || err != 0) {
            tcp_proxy_destroy(p);
            return;
        }
        p->connecting = false;
    }

    if (events & EPOLLERR) {
        tcp_proxy_destroy(p);
        return;
    }

    while (p->v2t_off < p->v2t_len) {
        ssize_t w = send(p->tcp_fd, p->v2t_buf + p->v2t_off,
                         p->v2t_len - p->v2t_off, MSG_NOSIGNAL);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            tcp_proxy_destroy(p);
            return;
        }
        p->v2t_off += (size_t)w;
    }
    if (p->v2t_off == p->v2t_len)
        p->v2t_off = p->v2t_len = 0;

    if (!p->vsock_eof && p->v2t_len == 0) {
        for (;;) {
            ssize_t n = recv(p->vsock_fd, p->v2t_buf, sizeof(p->v2t_buf), 0);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                p->vsock_eof = true;
                shutdown(p->tcp_fd, SHUT_WR);
                break;
            }
            if (n == 0) {
                p->vsock_eof = true;
                shutdown(p->tcp_fd, SHUT_WR);
                break;
            }
            p->v2t_off = 0;
            p->v2t_len = (size_t)n;
            while (p->v2t_off < p->v2t_len) {
                ssize_t w = send(p->tcp_fd, p->v2t_buf + p->v2t_off,
                                 p->v2t_len - p->v2t_off, MSG_NOSIGNAL);
                if (w < 0) {
                    if (errno == EINTR)
                        continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    tcp_proxy_destroy(p);
                    return;
                }
                p->v2t_off += (size_t)w;
            }
            if (p->v2t_off < p->v2t_len)
                break;
            p->v2t_off = p->v2t_len = 0;
        }
    }

    while (p->t2v_off < p->t2v_len) {
        ssize_t w = send(p->vsock_fd, p->t2v_buf + p->t2v_off,
                         p->t2v_len - p->t2v_off, MSG_NOSIGNAL);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            tcp_proxy_destroy(p);
            return;
        }
        p->t2v_off += (size_t)w;
    }
    if (p->t2v_off == p->t2v_len)
        p->t2v_off = p->t2v_len = 0;

    if (!p->tcp_eof && p->t2v_len == 0) {
        for (;;) {
            ssize_t n = recv(p->tcp_fd, p->t2v_buf, sizeof(p->t2v_buf), 0);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                p->tcp_eof = true;
                shutdown(p->vsock_fd, SHUT_WR);
                break;
            }
            if (n == 0) {
                p->tcp_eof = true;
                shutdown(p->vsock_fd, SHUT_WR);
                break;
            }
            p->t2v_off = 0;
            p->t2v_len = (size_t)n;
            while (p->t2v_off < p->t2v_len) {
                ssize_t w = send(p->vsock_fd, p->t2v_buf + p->t2v_off,
                                 p->t2v_len - p->t2v_off, MSG_NOSIGNAL);
                if (w < 0) {
                    if (errno == EINTR)
                        continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    tcp_proxy_destroy(p);
                    return;
                }
                p->t2v_off += (size_t)w;
            }
            if (p->t2v_off < p->t2v_len)
                break;
            p->t2v_off = p->t2v_len = 0;
        }
    }

    if (p->vsock_eof && p->tcp_eof && p->v2t_len == 0 && p->t2v_len == 0) {
        tcp_proxy_destroy(p);
        return;
    }

    tcp_proxy_update_events(p);
}

void sock_mirror_handle_incoming(int vsock_fd, uint32_t resource_id) {
    uint8_t family;
    uint16_t port;
    swiptermd_mirror_unpack_resource_id(resource_id, &family, &port);
    if (port == 0) {
        close(vsock_fd);
        return;
    }

    int af = (family == SWIPTERMD_MIRROR_AF_INET6) ? AF_INET6 : AF_INET;
    int tfd = guest_socket(af, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (tfd < 0) {
        close(vsock_fd);
        return;
    }

    int r;
    if (af == AF_INET) {
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        r = connect(tfd, (struct sockaddr *)&sa, sizeof(sa));
    } else {
        struct sockaddr_in6 sa6;
        memset(&sa6, 0, sizeof(sa6));
        sa6.sin6_family = AF_INET6;
        sa6.sin6_port = htons(port);
        sa6.sin6_addr = in6addr_loopback;
        r = connect(tfd, (struct sockaddr *)&sa6, sizeof(sa6));
    }

    if (r < 0 && errno != EINPROGRESS) {
        close(tfd);
        close(vsock_fd);
        return;
    }

    tcp_proxy_t *p = calloc(1, sizeof(*p));
    if (!p) {
        close(tfd);
        close(vsock_fd);
        return;
    }
    p->vsock_fd = vsock_fd;
    p->tcp_fd = tfd;
    p->connecting = (r < 0);

    ev_add(&p->vsock_ev, vsock_fd, p->connecting ? 0 : EPOLLIN, on_tcp_proxy_event, p);
    ev_add(&p->tcp_ev, tfd, p->connecting ? EPOLLOUT : EPOLLIN, on_tcp_proxy_event, p);
}
