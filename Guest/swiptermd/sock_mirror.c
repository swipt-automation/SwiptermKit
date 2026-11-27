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

typedef struct listener_set {
    listener_key_t *items;
    size_t count;
    size_t cap;
} listener_set_t;

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
static listener_set_t g_listeners;

static trace_cpu_t *g_trace_cpus;
static size_t g_trace_count;
static unsigned g_trace_offsets[TRACE_FIELD_COUNT];
static struct stat g_guest_net_stat;
static listen_start_t g_pending[MIRROR_PENDING_CAP];
static size_t g_pending_count;
static int g_settle_attempt = -1;

static const uint8_t any_addr[16];
static const uint8_t loopback4[4] = {127, 0, 0, 1};
static const uint8_t loopback6[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

static int listener_cmp(const void *a, const void *b) {
    const listener_key_t *x = a;
    const listener_key_t *y = b;
    if (x->family != y->family)
        return x->family < y->family ? -1 : 1;
    if (x->port != y->port)
        return x->port < y->port ? -1 : 1;
    return memcmp(x->addr, y->addr, 16);
}

static bool is_mirrorable_addr(uint8_t family, const uint8_t addr[16]) {
    if (family == SWIPTERMD_MIRROR_AF_INET) {
        if (addr[0] == 0 && addr[1] == 0 && addr[2] == 0 && addr[3] == 0)
            return true;
        if (addr[0] == 127)
            return true;
        return addr[0] == 10 && addr[1] == 0 && addr[2] == 2 && addr[3] == 15;
    }
    if (family == SWIPTERMD_MIRROR_AF_INET6) {
        static const uint8_t guest6[16] = {0xfd, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x15};
        return memcmp(addr, any_addr, 16) == 0 || memcmp(addr, loopback6, 16) == 0 || memcmp(addr, guest6, 16) == 0;
    }
    return false;
}

static bool scan_family(int af, listener_set_t *out) {
    uint8_t wire_family = af == AF_INET ? SWIPTERMD_MIRROR_AF_INET : SWIPTERMD_MIRROR_AF_INET6;
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
        return false;

    uint8_t buf[16384];
    for (;;) {
        ssize_t n = recv(g_diag_nl_fd, buf, sizeof(buf), 0);
        if (n <= 0)
            return false;

        for (struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
             NLMSG_OK(nlh, (unsigned int)n);
             nlh = NLMSG_NEXT(nlh, n)) {
            if (nlh->nlmsg_type == NLMSG_DONE)
                return true;
            if (nlh->nlmsg_type == NLMSG_ERROR)
                return false;
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

            if (out->count == out->cap) {
                size_t cap = out->cap ? out->cap * 2 : 64;
                listener_key_t *items = realloc(out->items, cap * sizeof(*items));
                if (!items)
                    return false;
                out->items = items;
                out->cap = cap;
            }
            out->items[out->count++] = key;
        }
    }
}

void sock_mirror_scan_now(void) {
    if (g_diag_nl_fd < 0)
        return;

    listener_set_t cur = {0};
    if (!scan_family(AF_INET, &cur) || !scan_family(AF_INET6, &cur)) {
        free(cur.items);
        return;
    }
    if (cur.count > 0) {
        qsort(cur.items, cur.count, sizeof(*cur.items), listener_cmp);
        size_t unique = 1;
        for (size_t i = 1; i < cur.count; i++) {
            if (listener_cmp(&cur.items[i], &cur.items[unique - 1]) != 0)
                cur.items[unique++] = cur.items[i];
        }
        cur.count = unique;
    }

    size_t wire_cap = 4 + (cur.count + g_listeners.count) * SWIPTERMD_MIRROR_ENTRY_WIRE_SIZE;
    uint8_t *wire = malloc(wire_cap);
    if (!wire) {
        free(cur.items);
        return;
    }
    swiptermd_buf_w_t w = {.data = wire, .cap = wire_cap};
    swiptermd_w_u32(&w, 0);
    uint32_t diff_count = 0;
    bool added = false;
    for (size_t i = 0, j = 0; i < cur.count || j < g_listeners.count;) {
        int order = i == cur.count            ? 1
                    : j == g_listeners.count ? -1
                                             : listener_cmp(&cur.items[i], &g_listeners.items[j]);
        if (order == 0) {
            i++;
            j++;
            continue;
        }
        const listener_key_t *key = order < 0 ? &cur.items[i++] : &g_listeners.items[j++];
        swiptermd_mirror_entry_t e = {
            .family = key->family,
            .action = order < 0 ? SWIPTERMD_MIRROR_ADDED : SWIPTERMD_MIRROR_REMOVED,
            .port = key->port,
        };
        memcpy(e.addr, key->addr, 16);
        swiptermd_mirror_entry_encode(&w, &e);
        added = added || order < 0;
        diff_count++;
    }

    if (added) {
        for (size_t i = g_pending_count; i-- > 0;) {
            if (g_pending[i].port == 0)
                g_pending[i] = g_pending[--g_pending_count];
        }
    }

    free(g_listeners.items);
    g_listeners = cur;

    if (diff_count > 0) {
        swiptermd_put_be32(wire, diff_count);
        ctrl_send_frame(SWIPTERMD_OP_MIRROR_UPDATE, 0, wire, (uint32_t)w.len);
    }
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
    close(p->vsock_fd);
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

static bool tcp_proxy_pump(int src_fd, int dst_fd, uint8_t *buf, size_t *off, size_t *len, bool *eof) {
    for (;;) {
        while (*off < *len) {
            ssize_t w = send(dst_fd, buf + *off, *len - *off, MSG_NOSIGNAL);
            if (w < 0) {
                if (errno == EINTR)
                    continue;
                return errno == EAGAIN || errno == EWOULDBLOCK;
            }
            *off += (size_t)w;
        }
        *off = *len = 0;
        if (*eof)
            return true;
        ssize_t n = recv(src_fd, buf, MIRROR_BUF_CAP, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return true;
        if (n <= 0) {
            *eof = true;
            shutdown(dst_fd, SHUT_WR);
            return true;
        }
        *len = (size_t)n;
    }
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

    if ((events & EPOLLERR) ||
        !tcp_proxy_pump(p->vsock_fd, p->tcp_fd, p->v2t_buf, &p->v2t_off, &p->v2t_len, &p->vsock_eof) ||
        !tcp_proxy_pump(p->tcp_fd, p->vsock_fd, p->t2v_buf, &p->t2v_off, &p->t2v_len, &p->tcp_eof) ||
        (p->vsock_eof && p->tcp_eof && p->v2t_len == 0 && p->t2v_len == 0)) {
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

    union {
        struct sockaddr sa;
        struct sockaddr_in in;
        struct sockaddr_in6 in6;
    } addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t addr_len;
    uint8_t *target;
    size_t target_len;
    const uint8_t *loopback;
    if (family == SWIPTERMD_MIRROR_AF_INET6) {
        addr.in6.sin6_family = AF_INET6;
        addr.in6.sin6_port = htons(port);
        addr_len = sizeof(addr.in6);
        target = addr.in6.sin6_addr.s6_addr;
        target_len = sizeof(addr.in6.sin6_addr);
        loopback = loopback6;
    } else {
        addr.in.sin_family = AF_INET;
        addr.in.sin_port = htons(port);
        addr_len = sizeof(addr.in);
        target = (uint8_t *)&addr.in.sin_addr;
        target_len = sizeof(addr.in.sin_addr);
        loopback = loopback4;
    }
    memcpy(target, loopback, target_len);
    for (size_t i = 0; i < g_listeners.count; i++) {
        const listener_key_t *l = &g_listeners.items[i];
        if (l->family != family || l->port != port)
            continue;
        if (memcmp(l->addr, any_addr, target_len) == 0 || memcmp(l->addr, loopback, target_len) == 0) {
            memcpy(target, loopback, target_len);
            break;
        }
        memcpy(target, l->addr, target_len);
    }

    int tfd = guest_socket(addr.sa.sa_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (tfd < 0) {
        close(vsock_fd);
        return;
    }
    int r = connect(tfd, &addr.sa, addr_len);
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

    if (ev_add(&p->vsock_ev, vsock_fd, p->connecting ? 0 : EPOLLIN, on_tcp_proxy_event, p) < 0 ||
        ev_add(&p->tcp_ev, tfd, p->connecting ? EPOLLOUT : EPOLLIN, on_tcp_proxy_event, p) < 0)
        tcp_proxy_destroy(p);
}
