#include "swiptermd.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/perf_event.h>
#include <linux/sock_diag.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <unistd.h>

#define MIRROR_MAX_LISTENERS 256
#define MIRROR_BUF_CAP 32768
#define TRACEFS_DIR "/sys/kernel/tracing"
#define LISTEN_EVENT_FORMAT TRACEFS_DIR "/events/sock/inet_sock_set_state/format"
#define LISTEN_EVENT_FILTER "protocol == 6 && (oldstate == 10 || newstate == 10)"

enum {
    MIRROR_POLL_NS = 500 * 1000 * 1000,
    MIRROR_SETTLE_NS = 2 * 1000 * 1000,
    MIRROR_SETTLE_ATTEMPTS = 8,
    MIRROR_PENDING_CAP = 64,
    TRACE_DATA_PAGES = 2,
    TRACE_RECORD_CAP = 256,
    TRACE_RAW_OFFSET = sizeof(struct perf_event_header) + 3 * sizeof(uint32_t),
};

enum {
    TRACE_SKADDR,
    TRACE_NEWSTATE,
    TRACE_SPORT,
    TRACE_FAMILY,
    TRACE_FIELD_COUNT,
};

static const struct {
    const char *name;
    unsigned size;
} trace_fields[TRACE_FIELD_COUNT] = {
    [TRACE_SKADDR] = {"skaddr", sizeof(uint64_t)},
    [TRACE_NEWSTATE] = {"newstate", sizeof(int32_t)},
    [TRACE_SPORT] = {"sport", sizeof(uint16_t)},
    [TRACE_FAMILY] = {"family", sizeof(uint16_t)},
};

typedef struct listener_key {
    uint8_t family;
    uint16_t port;
    uint8_t addr[16];
} listener_key_t;

typedef struct listen_start {
    uint64_t skaddr;
    uint16_t family;
    uint16_t port;
} listen_start_t;

typedef struct trace_cpu {
    int fd;
    struct perf_event_mmap_page *meta;
    ev_node_t ev;
} trace_cpu_t;

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

static trace_cpu_t *g_trace_cpus;
static size_t g_trace_count;
static unsigned g_trace_offsets[TRACE_FIELD_COUNT];
static struct stat g_guest_net_stat;
static listen_start_t g_pending[MIRROR_PENDING_CAP];
static size_t g_pending_count;
static int g_settle_attempt = -1;

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

            for (size_t i = g_pending_count; i-- > 0;) {
                if (g_pending[i].family == af && g_pending[i].port == port)
                    g_pending[i] = g_pending[--g_pending_count];
            }

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

    if (diff_count > 0) {
        for (size_t i = g_pending_count; i-- > 0;) {
            if (g_pending[i].port == 0)
                g_pending[i] = g_pending[--g_pending_count];
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

static void mirror_settle(int attempt) {
    g_settle_attempt = attempt;
    struct itimerspec its = {.it_value = {.tv_nsec = (long)MIRROR_SETTLE_NS << attempt}};
    timerfd_settime(g_scan_timer_fd, 0, &its, NULL);
}

static void on_scan_timer(ev_node_t *node, uint32_t events) {
    (void)events;
    uint64_t expirations;
    while (read(node->fd, &expirations, sizeof(expirations)) > 0) {}
    sock_mirror_scan_now();
    if (g_trace_count == 0)
        return;
    if (g_pending_count > 0 && g_settle_attempt + 1 < MIRROR_SETTLE_ATTEMPTS) {
        mirror_settle(g_settle_attempt + 1);
        return;
    }
    g_pending_count = 0;
    g_settle_attempt = -1;
}

static bool trace_note(const struct perf_event_header *hdr, const uint8_t *rec) {
    listen_start_t start = {0};
    if (hdr->type == PERF_RECORD_SAMPLE && hdr->size <= TRACE_RECORD_CAP) {
        const uint8_t *raw = rec + TRACE_RAW_OFFSET;
        int32_t newstate;
        memcpy(&newstate, raw + g_trace_offsets[TRACE_NEWSTATE], sizeof(newstate));
        memcpy(&start.skaddr, raw + g_trace_offsets[TRACE_SKADDR], sizeof(start.skaddr));
        if (newstate != TCP_LISTEN) {
            for (size_t i = g_pending_count; i-- > 0;) {
                if (g_pending[i].skaddr == start.skaddr)
                    g_pending[i] = g_pending[--g_pending_count];
            }
            return true;
        }
        uint32_t pid;
        uint32_t tid;
        memcpy(&pid, rec + sizeof(*hdr), sizeof(pid));
        memcpy(&tid, rec + sizeof(*hdr) + sizeof(pid), sizeof(tid));
        char path[48];
        snprintf(path, sizeof(path), "/proc/%" PRIu32 "/task/%" PRIu32 "/ns/net", pid, tid);
        struct stat st;
        if (stat(path, &st) == 0 && (st.st_dev != g_guest_net_stat.st_dev || st.st_ino != g_guest_net_stat.st_ino))
            return false;
        memcpy(&start.family, raw + g_trace_offsets[TRACE_FAMILY], sizeof(start.family));
        memcpy(&start.port, raw + g_trace_offsets[TRACE_SPORT], sizeof(start.port));
    }
    if (g_pending_count < MIRROR_PENDING_CAP)
        g_pending[g_pending_count++] = start;
    return true;
}

static void on_trace_ready(ev_node_t *node, uint32_t events) {
    (void)events;
    struct perf_event_mmap_page *meta = ((trace_cpu_t *)node->ctx)->meta;
    const uint8_t *ring = (const uint8_t *)meta + meta->data_offset;
    uint64_t mask = meta->data_size - 1;
    uint64_t head = __atomic_load_n(&meta->data_head, __ATOMIC_ACQUIRE);
    uint64_t tail = meta->data_tail;
    bool changed = false;
    while (tail < head) {
        struct perf_event_header hdr;
        memcpy(&hdr, ring + (tail & mask), sizeof(hdr));
        uint8_t rec[TRACE_RECORD_CAP];
        size_t len = hdr.size < sizeof(rec) ? hdr.size : sizeof(rec);
        size_t first = mask + 1 - (tail & mask);
        if (first > len)
            first = len;
        memcpy(rec, ring + (tail & mask), first);
        memcpy(rec + first, ring, len - first);
        changed |= trace_note(&hdr, rec);
        tail += hdr.size;
    }
    __atomic_store_n(&meta->data_tail, tail, __ATOMIC_RELEASE);
    if (changed && g_settle_attempt != 0)
        mirror_settle(0);
}

static uint64_t trace_load_format(void) {
    bool mounted = mount("tracefs", TRACEFS_DIR, "tracefs", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) == 0;
    FILE *fp = fopen(LISTEN_EVENT_FORMAT, "re");
    uint64_t id = 0;
    unsigned found = 0;
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp) && strncmp(line, "print fmt:", 10) != 0) {
            if (sscanf(line, "ID: %" SCNu64, &id) == 1)
                continue;
            char *decl = strstr(line, "field:");
            char *end = decl ? strchr(decl, ';') : NULL;
            unsigned offset;
            unsigned size;
            if (!end || sscanf(end + 1, " offset:%u; size:%u;", &offset, &size) != 2)
                continue;
            *end = '\0';
            char *name = end;
            while (name > decl && name[-1] != ' ' && name[-1] != '*')
                name--;
            name[strcspn(name, "[")] = '\0';
            for (size_t i = 0; i < TRACE_FIELD_COUNT; i++) {
                if (strcmp(name, trace_fields[i].name) == 0 && size == trace_fields[i].size &&
                    offset + size <= TRACE_RECORD_CAP - TRACE_RAW_OFFSET) {
                    g_trace_offsets[i] = offset;
                    found |= 1U << i;
                }
            }
        }
        fclose(fp);
    }
    if (mounted)
        umount2(TRACEFS_DIR, 0);
    return found == (1U << TRACE_FIELD_COUNT) - 1 ? id : 0;
}

static bool trace_start(void) {
    uint64_t id = trace_load_format();
    cpu_set_t cpus;
    if (id == 0 || fstat(network_guest_fd(), &g_guest_net_stat) < 0 || sched_getaffinity(0, sizeof(cpus), &cpus) < 0)
        return false;
    g_trace_cpus = calloc((size_t)CPU_COUNT(&cpus), sizeof(*g_trace_cpus));
    if (!g_trace_cpus)
        return false;

    size_t map_len = (size_t)(1 + TRACE_DATA_PAGES) * (size_t)sysconf(_SC_PAGESIZE);
    struct perf_event_attr attr = {
        .type = PERF_TYPE_TRACEPOINT,
        .size = sizeof(attr),
        .config = id,
        .sample_period = 1,
        .sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_RAW,
        .wakeup_events = 1,
    };
    for (int cpu = 0; cpu < CPU_SETSIZE; cpu++) {
        if (!CPU_ISSET(cpu, &cpus))
            continue;
        trace_cpu_t *c = &g_trace_cpus[g_trace_count];
        c->fd = (int)syscall(SYS_perf_event_open, &attr, -1, cpu, -1, PERF_FLAG_FD_CLOEXEC);
        if (c->fd < 0)
            goto fail;
        g_trace_count++;
        if (ioctl(c->fd, PERF_EVENT_IOC_SET_FILTER, LISTEN_EVENT_FILTER) < 0)
            goto fail;
        void *meta = mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, c->fd, 0);
        if (meta == MAP_FAILED)
            goto fail;
        c->meta = meta;
        if (ev_add(&c->ev, c->fd, EPOLLIN, on_trace_ready, c) < 0)
            goto fail;
    }
    return true;

fail:
    while (g_trace_count > 0) {
        trace_cpu_t *c = &g_trace_cpus[--g_trace_count];
        ev_del(&c->ev);
        if (c->meta)
            munmap(c->meta, map_len);
        close(c->fd);
    }
    free(g_trace_cpus);
    g_trace_cpus = NULL;
    return false;
}

void sock_mirror_init(void) {
    g_diag_nl_fd = guest_socket(AF_NETLINK, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, NETLINK_SOCK_DIAG);
    g_scan_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (g_diag_nl_fd < 0 || g_scan_timer_fd < 0 ||
        ev_add(&g_scan_timer_ev, g_scan_timer_fd, EPOLLIN, on_scan_timer, NULL) < 0)
        return;
    if (!trace_start()) {
        struct itimerspec its = {
            .it_interval = {.tv_nsec = MIRROR_POLL_NS},
            .it_value = {.tv_nsec = MIRROR_POLL_NS},
        };
        timerfd_settime(g_scan_timer_fd, 0, &its, NULL);
    }
    sock_mirror_scan_now();
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
        for (size_t i = 0; i < g_prev_count; i++) {
            const listener_key_t *l = &g_prev_listeners[i];
            if (l->family != SWIPTERMD_MIRROR_AF_INET || l->port != port)
                continue;
            uint32_t bound;
            memcpy(&bound, l->addr, sizeof(bound));
            if (bound == htonl(INADDR_ANY) || bound == htonl(INADDR_LOOPBACK)) {
                sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                break;
            }
            sa.sin_addr.s_addr = bound;
        }
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
