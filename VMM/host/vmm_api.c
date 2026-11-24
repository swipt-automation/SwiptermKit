#define _DARWIN_C_SOURCE

#include "../../Guest/swiptermd/wire_format.h"
#include "host_internal.h"
#include "rpc_codec.h"
#include "../board/machine.h"
#include "../include/CSwiptermKit/swipterm.h"

#include <CommonCrypto/CommonDigest.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <unistd.h>

enum {
    TERMINAL_RING_CAPACITY = 256 * 1024,
    CONTROL_BOOT_TIMEOUT_MS = 90000,
    SHUTDOWN_TIMEOUT_SEC = 30,
    STOP_REPORT_TIMEOUT_SEC = 1,
    SUSPEND_SYNC_TIMEOUT_SEC = 3,
    MAX_DEV_READ_CHUNK = 1024 * 1024,
    FS_READ_CHUNK_SIZE = 1024 * 1024,
    FS_WRITE_CHUNK_SIZE = 4 * 1024 * 1024,
};

typedef struct pending_req {
    uint32_t req_id;
    bool done;
    bool failed;
    uint8_t *reply_body;
    uint32_t reply_len;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct pending_req *next;
} pending_req_t;

typedef enum stream_dir {
    STREAM_DIR_HOST_TO_GUEST = 1,
    STREAM_DIR_GUEST_TO_HOST = 2,
} stream_dir_t;

typedef struct stream_done_signal {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int remaining;
} stream_done_signal_t;

typedef struct stream_route {
    uint32_t role;
    uint64_t token;
    int peer_fd;
    int vsock_fd;
    stream_dir_t dir;
    swipterm_invocation *inv;
    struct stream_route *next;
} stream_route_t;

typedef struct mount_entry {
    char *guest_path;
    char tag[32];
    uint32_t slot;
    struct mount_entry *next;
} mount_entry_t;

typedef struct command_entry {
    char *name;
    void *user_context;
    unsigned users;
    struct command_entry *next;
} command_entry_t;

typedef struct device_entry {
    uint32_t dev_id;
    void *user_context;
    unsigned users;
    struct device_entry *next;
} device_entry_t;

typedef struct device_file_entry {
    uint32_t dev_id;
    uint64_t file_id;
    void *file_ptr;
    void (*device_close)(void *);
    _Atomic int refcount;
    struct device_file_entry *next;
} device_file_entry_t;

struct swipterm_process {
    pthread_mutex_t rt_lock;
    swipterm_runtime *rt;
    uint32_t proc_id;
    int pid;
    void *user_context;
    swipterm_terminal *terminal;
    _Atomic int refcount;
    _Atomic bool exited;
    bool spawned;
    struct swipterm_process *next;
};

struct swipterm_terminal {
    pthread_mutex_t rt_lock;
    swipterm_runtime *rt;
    uint32_t pty_id;
    uint64_t token;
    _Atomic int refcount;

    pthread_mutex_t lock;
    pthread_cond_t ring_cond;
    pthread_cond_t attach_cond;
    pthread_mutex_t write_lock;

    uint8_t ring[TERMINAL_RING_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;

    int notify_pipe[2];
    int data_fd;
    bool attached;
    bool eof;
    bool closed;
    bool in_use;

    struct swipterm_terminal *next;
};

struct swipterm_invocation {
    pthread_mutex_t rt_lock;
    swipterm_runtime *rt;
    void (*command_release)(void *);
    uint32_t inv_id;
    void *swift_context;
    bool cancelled;
    _Atomic int refcount;
    stream_done_signal_t out_flush;
    struct swipterm_invocation *next;
};

struct swipterm_runtime {
    vmm_machine_t *vm;
    virtio_dev_t *vsock;
    swipterm_network *network;
    swipterm_callbacks callbacks;
    bool console_log;

    int root_lock_fd;
    _Atomic int control_fd;
    int shutdown_pipe[2];
    pthread_mutex_t control_tx_lock;
    pthread_t control_rx_thread;
    bool control_rx_started;

    _Atomic bool booting;
    _Atomic bool halt_initiated;
    _Atomic bool shutting_down;
    _Atomic bool finalized;
    _Atomic bool halted;
    _Atomic bool vm_powered_off;
    _Atomic uint32_t next_req_id;
    _Atomic uint64_t next_token;
    _Atomic uint32_t next_proc_id;
    _Atomic uint32_t next_pty_id;
    _Atomic uint32_t next_mount_id;
    _Atomic uint32_t next_dev_id;
    _Atomic uint64_t next_file_id;

    pthread_mutex_t state_lock;
    pthread_cond_t hello_cond;
    pthread_cond_t halt_cond;
    pthread_cond_t ready_cond;
    pthread_cond_t console_cond;
    pthread_cond_t workers_cond;
    size_t active_workers;
    bool hello_done;
    bool console_active;
    bool control_rx_done;
    uint64_t session_nonce;
    uint32_t guest_caps;
    uint32_t ready_mask;
    swipterm_halt_info halt_info;
    swipterm_boot_info boot_info;

    pending_req_t *pending_reqs;
    stream_route_t *routes;
    swipterm_process *processes;
    swipterm_terminal *terminals;
    mount_entry_t *mounts;
    command_entry_t *commands;
    swipterm_invocation *invocations;
    device_entry_t *devices;
    device_file_entry_t *device_files;
};

static bool guest_has_capability(swipterm_runtime *rt, uint32_t capability) {
    pthread_mutex_lock(&rt->state_lock);
    bool supported = (rt->guest_caps & capability) != 0;
    pthread_mutex_unlock(&rt->state_lock);
    return supported;
}

static void set_nosigpipe(int fd) {
    int one = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
}

static void set_cloexec(int fd) {
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
}

static void process_release_internal(swipterm_process *p) {
    if (atomic_fetch_sub(&p->refcount, 1) == 1) {
        pthread_mutex_destroy(&p->rt_lock);
        free(p);
    }
}

static void terminal_release_internal(swipterm_terminal *t) {
    if (atomic_fetch_sub(&t->refcount, 1) == 1) {
        if (t->data_fd >= 0) {
            close(t->data_fd);
        }
        close(t->notify_pipe[0]);
        close(t->notify_pipe[1]);
        pthread_mutex_destroy(&t->rt_lock);
        pthread_mutex_destroy(&t->lock);
        pthread_cond_destroy(&t->ring_cond);
        pthread_cond_destroy(&t->attach_cond);
        pthread_mutex_destroy(&t->write_lock);
        free(t);
    }
}

static void invocation_release_internal(swipterm_invocation *inv) {
    if (atomic_fetch_sub(&inv->refcount, 1) == 1) {
        if (inv->swift_context && inv->command_release) {
            inv->command_release(inv->swift_context);
        }
        pthread_mutex_destroy(&inv->rt_lock);
        pthread_mutex_destroy(&inv->out_flush.lock);
        pthread_cond_destroy(&inv->out_flush.cond);
        free(inv);
    }
}

static void device_file_release(device_file_entry_t *fe) {
    if (atomic_fetch_sub(&fe->refcount, 1) == 1) {
        if (fe->file_ptr && fe->device_close) {
            fe->device_close(fe->file_ptr);
        }
        free(fe);
    }
}

static bool worker_try_enter(swipterm_runtime *rt) {
    pthread_mutex_lock(&rt->state_lock);
    if (atomic_load(&rt->shutting_down)) {
        pthread_mutex_unlock(&rt->state_lock);
        return false;
    }
    rt->active_workers++;
    pthread_mutex_unlock(&rt->state_lock);
    return true;
}

static void worker_leave(swipterm_runtime *rt) {
    pthread_mutex_lock(&rt->state_lock);
    rt->active_workers--;
    if (rt->active_workers == 0) {
        pthread_cond_broadcast(&rt->workers_cond);
    }
    pthread_mutex_unlock(&rt->state_lock);
}

static swipterm_runtime *enter_child_runtime(pthread_mutex_t *rt_lock, swipterm_runtime **rt_slot) {
    pthread_mutex_lock(rt_lock);
    swipterm_runtime *rt = *rt_slot;
    if (rt && !worker_try_enter(rt)) {
        rt = NULL;
    }
    pthread_mutex_unlock(rt_lock);
    return rt;
}

static bool control_send(swipterm_runtime *rt, uint16_t opcode, uint32_t req_id,
                         const void *body, uint32_t body_len) {
    pthread_mutex_lock(&rt->control_tx_lock);
    bool ok = (rt->control_fd >= 0) && rpc_send_frame(rt->control_fd, opcode, req_id, body, body_len);
    pthread_mutex_unlock(&rt->control_tx_lock);
    return ok;
}

static bool pending_req_withdraw(swipterm_runtime *rt, pending_req_t *req) {
    bool removed = false;
    pthread_mutex_lock(&rt->state_lock);
    for (pending_req_t **pp = &rt->pending_reqs; *pp; pp = &(*pp)->next) {
        if (*pp == req) {
            *pp = req->next;
            removed = true;
            break;
        }
    }
    pthread_mutex_unlock(&rt->state_lock);
    if (!removed) {
        pthread_mutex_lock(&req->lock);
        while (!req->done) {
            pthread_cond_wait(&req->cond, &req->lock);
        }
        pthread_mutex_unlock(&req->lock);
    }
    return removed;
}

static int control_rpc(swipterm_runtime *rt, uint16_t opcode,
                       const void *req_body, uint32_t req_len,
                       uint8_t **reply_body_out, uint32_t *reply_len_out,
                       const struct timespec *deadline) {
    if (!rt || atomic_load(&rt->shutting_down)) {
        return -LINUX_EIO;
    }

    pending_req_t req = {
        .req_id = atomic_fetch_add(&rt->next_req_id, 1) + 1,
    };
    pthread_mutex_init(&req.lock, NULL);
    pthread_cond_init(&req.cond, NULL);

    pthread_mutex_lock(&rt->state_lock);
    if (atomic_load(&rt->shutting_down) || atomic_load(&rt->halted) || atomic_load(&rt->vm_powered_off)) {
        pthread_mutex_unlock(&rt->state_lock);
        pthread_mutex_destroy(&req.lock);
        pthread_cond_destroy(&req.cond);
        return -LINUX_EIO;
    }
    req.next = rt->pending_reqs;
    rt->pending_reqs = &req;
    pthread_mutex_unlock(&rt->state_lock);

    if (!control_send(rt, opcode, req.req_id, req_body, req_len)) {
        (void)pending_req_withdraw(rt, &req);
        free(req.reply_body);
        pthread_mutex_destroy(&req.lock);
        pthread_cond_destroy(&req.cond);
        return -LINUX_EIO;
    }

    bool timed_out = false;
    pthread_mutex_lock(&req.lock);
    while (!req.done && !timed_out) {
        if (deadline) {
            timed_out = pthread_cond_timedwait(&req.cond, &req.lock, deadline) == ETIMEDOUT;
        } else {
            pthread_cond_wait(&req.cond, &req.lock);
        }
    }
    pthread_mutex_unlock(&req.lock);
    if (timed_out && pending_req_withdraw(rt, &req)) {
        pthread_mutex_destroy(&req.lock);
        pthread_cond_destroy(&req.cond);
        return -LINUX_ETIMEDOUT;
    }

    pthread_mutex_destroy(&req.lock);
    pthread_cond_destroy(&req.cond);

    if (req.failed) {
        free(req.reply_body);
        return -LINUX_EIO;
    }
    if (reply_body_out) {
        *reply_body_out = req.reply_body;
    } else {
        free(req.reply_body);
    }
    if (reply_len_out) {
        *reply_len_out = req.reply_len;
    }
    return 0;
}

static int control_rpc_status(swipterm_runtime *rt, uint16_t opcode,
                              const void *req_body, uint32_t req_len) {
    uint8_t *reply = NULL;
    uint32_t rlen = 0;
    int rc = control_rpc(rt, opcode, req_body, req_len, &reply, &rlen, NULL);
    if (rc < 0) {
        return rc;
    }
    int32_t status = -LINUX_EIO;
    if (!swiptermd_dec_err(reply, rlen, &status)) {
        status = -LINUX_EIO;
    }
    free(reply);
    return status;
}

typedef struct pump_args {
    swipterm_runtime *rt;
    int src_fd;
    int dst_fd;
    bool shutdown_dst_wr;
    swipterm_invocation *inv;
} pump_args_t;

static void signal_invocation_flush(swipterm_invocation *inv) {
    if (!inv) {
        return;
    }
    pthread_mutex_lock(&inv->out_flush.lock);
    inv->out_flush.remaining--;
    if (inv->out_flush.remaining <= 0) {
        pthread_cond_broadcast(&inv->out_flush.cond);
    }
    pthread_mutex_unlock(&inv->out_flush.lock);
    invocation_release_internal(inv);
}

static void *stream_pump_thread(void *arg) {
    pump_args_t *pa = arg;
    swipterm_runtime *rt = pa->rt;
    int src = pa->src_fd;
    int dst = pa->dst_fd;
    bool shut_wr = pa->shutdown_dst_wr;
    swipterm_invocation *inv = pa->inv;
    free(pa);

    int dst_fl = fcntl(dst, F_GETFL, 0);
    if (dst_fl >= 0) {
        (void)fcntl(dst, F_SETFL, dst_fl | O_NONBLOCK);
    }

    int src_fl = fcntl(src, F_GETFL, 0);
    bool nonblocking = src_fl >= 0 && fcntl(src, F_SETFL, src_fl | O_NONBLOCK) == 0;
    uint8_t buf[128 * 1024];
    while (!atomic_load(&rt->shutting_down)) {
        ssize_t n = -1;
        if (nonblocking) {
            n = read(src, buf, sizeof(buf));
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                break;
            }
        }
        if (n < 0) {
            struct pollfd pfds[2] = {
                {.fd = src, .events = POLLIN},
                {.fd = rt->shutdown_pipe[0], .events = POLLIN},
            };
            int ready = poll(pfds, 2, -1);
            if (ready < 0 && errno == EINTR) {
                continue;
            }
            if (ready < 0 || pfds[1].revents != 0 || atomic_load(&rt->shutting_down)) {
                break;
            }
            if (nonblocking) {
                continue;
            }
            if (!(pfds[0].revents & (POLLIN | POLLHUP | POLLERR))) {
                break;
            }
            n = read(src, buf, sizeof(buf));
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n < 0) {
                break;
            }
        }
        if (n == 0) {
            break;
        }
        size_t off = 0;
        bool write_ok = true;
        while (off < (size_t)n) {
            if (atomic_load(&rt->shutting_down)) {
                write_ok = false;
                break;
            }
            ssize_t w = write(dst, buf + off, (size_t)n - off);
            if (w > 0) {
                off += (size_t)w;
                continue;
            }
            if (w < 0 && errno == EINTR) {
                continue;
            }
            if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                struct pollfd wpfds[2] = {
                    {.fd = dst, .events = POLLOUT},
                    {.fd = rt->shutdown_pipe[0], .events = POLLIN},
                };
                int wr = poll(wpfds, 2, -1);
                if (wr < 0 && errno == EINTR) {
                    continue;
                }
                if (wr <= 0 || wpfds[1].revents != 0 || (wpfds[0].revents & (POLLERR | POLLHUP | POLLNVAL))) {
                    write_ok = false;
                    break;
                }
                continue;
            }
            write_ok = false;
            break;
        }
        if (!write_ok) {
            break;
        }
    }
    if (shut_wr) {
        (void)shutdown(dst, SHUT_WR);
    }
    close(src);
    close(dst);

    signal_invocation_flush(inv);
    worker_leave(rt);
    return NULL;
}

static void launch_stream_pump(swipterm_runtime *rt, int peer_fd, int vsock_fd, stream_dir_t dir,
                               swipterm_invocation *inv) {
    if (!worker_try_enter(rt)) {
        close(peer_fd);
        close(vsock_fd);
        signal_invocation_flush(inv);
        return;
    }
    pump_args_t *pa = malloc(sizeof(*pa));
    if (!pa) {
        close(peer_fd);
        close(vsock_fd);
        signal_invocation_flush(inv);
        worker_leave(rt);
        return;
    }
    pa->rt = rt;
    if (dir == STREAM_DIR_HOST_TO_GUEST) {
        pa->src_fd = peer_fd;
        pa->dst_fd = vsock_fd;
        pa->shutdown_dst_wr = true;
    } else {
        pa->src_fd = vsock_fd;
        pa->dst_fd = peer_fd;
        pa->shutdown_dst_wr = false;
    }
    pa->inv = inv;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t tid;
    if (pthread_create(&tid, &attr, stream_pump_thread, pa) != 0) {
        free(pa);
        close(peer_fd);
        close(vsock_fd);
        signal_invocation_flush(inv);
        worker_leave(rt);
    }
    pthread_attr_destroy(&attr);
}

static bool register_stream_route(swipterm_runtime *rt, uint32_t role, uint64_t token, int peer_fd,
                                  stream_dir_t dir, swipterm_invocation *inv) {
    if (inv) {
        atomic_fetch_add(&inv->refcount, 1);
    }
    pthread_mutex_lock(&rt->state_lock);
    for (stream_route_t **pp = &rt->routes; *pp; pp = &(*pp)->next) {
        stream_route_t *r = *pp;
        if (r->role == role && r->token == token && r->vsock_fd >= 0 && r->peer_fd < 0) {
            int vsock_fd = r->vsock_fd;
            *pp = r->next;
            free(r);
            pthread_mutex_unlock(&rt->state_lock);
            launch_stream_pump(rt, peer_fd, vsock_fd, dir, inv);
            return true;
        }
    }
    if (atomic_load(&rt->shutting_down)) {
        pthread_mutex_unlock(&rt->state_lock);
        if (inv) {
            invocation_release_internal(inv);
        }
        return false;
    }
    stream_route_t *r = calloc(1, sizeof(*r));
    if (!r) {
        pthread_mutex_unlock(&rt->state_lock);
        if (inv) {
            invocation_release_internal(inv);
        }
        return false;
    }
    r->role = role;
    r->token = token;
    r->peer_fd = peer_fd;
    r->vsock_fd = -1;
    r->dir = dir;
    r->inv = inv;
    r->next = rt->routes;
    rt->routes = r;
    pthread_mutex_unlock(&rt->state_lock);
    return true;
}

static void cancel_stream_route(swipterm_runtime *rt, uint32_t role, uint64_t token) {
    swipterm_invocation *inv = NULL;
    pthread_mutex_lock(&rt->state_lock);
    for (stream_route_t **pp = &rt->routes; *pp; pp = &(*pp)->next) {
        stream_route_t *r = *pp;
        if (r->role == role && r->token == token) {
            *pp = r->next;
            if (r->peer_fd >= 0) {
                close(r->peer_fd);
            }
            if (r->vsock_fd >= 0) {
                close(r->vsock_fd);
            }
            inv = r->inv;
            free(r);
            break;
        }
    }
    pthread_mutex_unlock(&rt->state_lock);
    if (inv) {
        signal_invocation_flush(inv);
    }
}

static void attach_vsock_to_route(swipterm_runtime *rt, uint32_t role, uint64_t token, int vsock_fd) {
    pthread_mutex_lock(&rt->state_lock);
    for (stream_route_t **pp = &rt->routes; *pp; pp = &(*pp)->next) {
        stream_route_t *r = *pp;
        if (r->role == role && r->token == token && r->peer_fd >= 0 && r->vsock_fd < 0) {
            int peer_fd = r->peer_fd;
            stream_dir_t dir = r->dir;
            swipterm_invocation *inv = r->inv;
            *pp = r->next;
            free(r);
            pthread_mutex_unlock(&rt->state_lock);
            launch_stream_pump(rt, peer_fd, vsock_fd, dir, inv);
            return;
        }
    }
    if (atomic_load(&rt->shutting_down)) {
        pthread_mutex_unlock(&rt->state_lock);
        close(vsock_fd);
        return;
    }
    stream_route_t *r = calloc(1, sizeof(*r));
    if (!r) {
        pthread_mutex_unlock(&rt->state_lock);
        close(vsock_fd);
        return;
    }
    r->role = role;
    r->token = token;
    r->peer_fd = -1;
    r->vsock_fd = vsock_fd;
    r->next = rt->routes;
    rt->routes = r;
    pthread_mutex_unlock(&rt->state_lock);
}

static void terminal_notify(swipterm_terminal *t) {
    uint8_t b = 1;
    (void)write(t->notify_pipe[1], &b, 1);
}

static void terminal_run_pty_stream(swipterm_terminal *t, int vsock_fd) {
    pthread_mutex_lock(&t->lock);
    if (t->closed) {
        pthread_mutex_unlock(&t->lock);
        close(vsock_fd);
        return;
    }
    t->data_fd = vsock_fd;
    t->attached = true;
    pthread_cond_broadcast(&t->attach_cond);
    pthread_mutex_unlock(&t->lock);

    for (;;) {
        pthread_mutex_lock(&t->lock);
        while (!t->closed && t->count == TERMINAL_RING_CAPACITY) {
            pthread_cond_wait(&t->ring_cond, &t->lock);
        }
        if (t->closed) {
            pthread_mutex_unlock(&t->lock);
            break;
        }
        size_t space = TERMINAL_RING_CAPACITY - t->count;
        size_t first = TERMINAL_RING_CAPACITY - t->tail;
        if (first > space) {
            first = space;
        }
        struct iovec spans[] = {
            {.iov_base = t->ring + t->tail, .iov_len = first},
            {.iov_base = t->ring, .iov_len = space - first},
        };
        pthread_mutex_unlock(&t->lock);

        ssize_t n = readv(vsock_fd, spans, spans[1].iov_len ? 2 : 1);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            break;
        }

        pthread_mutex_lock(&t->lock);
        t->tail = (t->tail + (size_t)n) % TERMINAL_RING_CAPACITY;
        t->count += (size_t)n;
        pthread_mutex_unlock(&t->lock);
        terminal_notify(t);
    }

    pthread_mutex_lock(&t->lock);
    t->eof = true;
    pthread_cond_broadcast(&t->attach_cond);
    pthread_mutex_unlock(&t->lock);
    terminal_notify(t);
}

typedef struct data_accept_args {
    swipterm_runtime *rt;
    int host_fd;
} data_accept_args_t;

static void *data_stream_thread(void *arg) {
    data_accept_args_t *da = arg;
    swipterm_runtime *rt = da->rt;
    int fd = da->host_fd;
    free(da);

    set_nosigpipe(fd);
    uint8_t raw[SWIPTERMD_ATTACH_HEADER_SIZE];
    if (!rpc_read_all(fd, raw, sizeof(raw))) {
        close(fd);
        worker_leave(rt);
        return NULL;
    }
    swiptermd_attach_hdr_t hdr;
    swiptermd_attach_hdr_decode(raw, &hdr);

    pthread_mutex_lock(&rt->state_lock);
    bool valid_session = rt->hello_done && hdr.session_nonce == rt->session_nonce;
    pthread_mutex_unlock(&rt->state_lock);
    if (!valid_session) {
        close(fd);
        worker_leave(rt);
        return NULL;
    }

    switch ((swiptermd_stream_role_t)hdr.role) {
    case SWIPTERMD_STREAM_CONSOLE: {
        uint8_t buf[4096];
        for (;;) {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            if (n == 0) {
                break;
            }
            if (rt->callbacks.console_output) {
                rt->callbacks.console_output(rt->callbacks.context, buf, (size_t)n);
            }
        }
        close(fd);
        pthread_mutex_lock(&rt->state_lock);
        rt->console_active = false;
        pthread_cond_broadcast(&rt->console_cond);
        pthread_mutex_unlock(&rt->state_lock);
        break;
    }
    case SWIPTERMD_STREAM_PTY: {
        swipterm_terminal *term = NULL;
        pthread_mutex_lock(&rt->state_lock);
        for (swipterm_terminal *t = rt->terminals; t != NULL; t = t->next) {
            if (t->token == hdr.token || (hdr.resource_id != 0 && t->pty_id == hdr.resource_id)) {
                term = t;
                atomic_fetch_add(&term->refcount, 1);
                break;
            }
        }
        pthread_mutex_unlock(&rt->state_lock);
        if (!term) {
            close(fd);
            break;
        }
        terminal_run_pty_stream(term, fd);
        terminal_release_internal(term);
        break;
    }
    case SWIPTERMD_STREAM_STDIO_IN:
    case SWIPTERMD_STREAM_STDIO_OUT:
    case SWIPTERMD_STREAM_STDIO_ERR:
    case SWIPTERMD_STREAM_CMD_IN:
    case SWIPTERMD_STREAM_CMD_OUT:
    case SWIPTERMD_STREAM_CMD_ERR:
        attach_vsock_to_route(rt, hdr.role, hdr.token, fd);
        break;
    default:
        close(fd);
        break;
    }
    worker_leave(rt);
    return NULL;
}

static void on_data_stream_accept(void *ctx, int host_fd, uint32_t guest_port, uint32_t host_port) {
    (void)guest_port;
    (void)host_port;
    swipterm_runtime *rt = ctx;
    if (!worker_try_enter(rt)) {
        close(host_fd);
        return;
    }
    data_accept_args_t *da = malloc(sizeof(*da));
    if (!da) {
        close(host_fd);
        worker_leave(rt);
        return;
    }
    da->rt = rt;
    da->host_fd = host_fd;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t tid;
    if (pthread_create(&tid, &attr, data_stream_thread, da) != 0) {
        free(da);
        close(host_fd);
        worker_leave(rt);
    }
    pthread_attr_destroy(&attr);
}

typedef struct guest_rpc_task {
    swipterm_runtime *rt;
    swiptermd_frame_hdr_t hdr;
    uint8_t *body;
} guest_rpc_task_t;

static command_entry_t *command_find(swipterm_runtime *rt, const char *name) {
    for (command_entry_t *c = rt->commands; c != NULL; c = c->next) {
        if (strcmp(c->name, name) == 0) {
            return c;
        }
    }
    return NULL;
}

static void registration_unuse(swipterm_runtime *rt, unsigned *users) {
    if (--*users == 0) {
        pthread_cond_broadcast(&rt->workers_cond);
    }
}

static void registration_wait_unused(swipterm_runtime *rt, const unsigned *users) {
    while (*users > 0) {
        pthread_cond_wait(&rt->workers_cond, &rt->state_lock);
    }
}

static void handle_cmd_start(swipterm_runtime *rt, uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);

    uint32_t inv_id = swiptermd_r_u32(&r);
    uint64_t stream_token = swiptermd_r_u64(&r);
    char *name = NULL;
    char *cwd = NULL;
    size_t argc = 0;
    char **argv = NULL;
    size_t envc = 0;
    char **envp = NULL;

    bool parsed = !r.underflow &&
                  rpc_r_str_dup(&r, &name) &&
                  rpc_r_str_dup(&r, &cwd) &&
                  rpc_r_strv_dup(&r, &argc, &argv) &&
                  rpc_r_strv_dup(&r, &envc, &envp);

    uint8_t err_reply[SWIPTERMD_ERR_REPLY_SIZE];

    if (!parsed || !rt->callbacks.command_start) {
        if (parsed) {
            cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_IN, stream_token);
            cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_OUT, stream_token);
            cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_ERR, stream_token);
        }
        swiptermd_enc_err(err_reply, -LINUX_EINVAL);
        control_send(rt, SWIPTERMD_OP_CMD_START | SWIPTERMD_OP_REPLY_FLAG, req_id, err_reply, sizeof(err_reply));
        free(name);
        free(cwd);
        rpc_free_strv(argv, argc);
        rpc_free_strv(envp, envc);
        return;
    }

    pthread_mutex_lock(&rt->state_lock);
    command_entry_t *command = command_find(rt, name);
    pthread_mutex_unlock(&rt->state_lock);

    if (!command) {
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_IN, stream_token);
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_OUT, stream_token);
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_ERR, stream_token);
        swiptermd_enc_err(err_reply, -LINUX_ENOENT);
        control_send(rt, SWIPTERMD_OP_CMD_START | SWIPTERMD_OP_REPLY_FLAG, req_id, err_reply, sizeof(err_reply));
        free(name);
        free(cwd);
        rpc_free_strv(argv, argc);
        rpc_free_strv(envp, envc);
        return;
    }

    int sp_in[2] = {-1, -1};
    int sp_out[2] = {-1, -1};
    int sp_err[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp_in) != 0 ||
        socketpair(AF_UNIX, SOCK_STREAM, 0, sp_out) != 0 ||
        socketpair(AF_UNIX, SOCK_STREAM, 0, sp_err) != 0) {
        for (int i = 0; i < 2; i++) {
            if (sp_in[i] >= 0) close(sp_in[i]);
            if (sp_out[i] >= 0) close(sp_out[i]);
            if (sp_err[i] >= 0) close(sp_err[i]);
        }
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_IN, stream_token);
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_OUT, stream_token);
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_ERR, stream_token);
        swiptermd_enc_err(err_reply, -LINUX_ENOMEM);
        control_send(rt, SWIPTERMD_OP_CMD_START | SWIPTERMD_OP_REPLY_FLAG, req_id, err_reply, sizeof(err_reply));
        free(name);
        free(cwd);
        rpc_free_strv(argv, argc);
        rpc_free_strv(envp, envc);
        return;
    }
    int all_fds[6] = {sp_in[0], sp_in[1], sp_out[0], sp_out[1], sp_err[0], sp_err[1]};
    for (int i = 0; i < 6; i++) {
        set_nosigpipe(all_fds[i]);
        set_cloexec(all_fds[i]);
    }

    swipterm_invocation *inv = calloc(1, sizeof(*inv));
    if (!inv) {
        for (int i = 0; i < 6; i++) close(all_fds[i]);
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_IN, stream_token);
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_OUT, stream_token);
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_ERR, stream_token);
        swiptermd_enc_err(err_reply, -LINUX_ENOMEM);
        control_send(rt, SWIPTERMD_OP_CMD_START | SWIPTERMD_OP_REPLY_FLAG, req_id, err_reply, sizeof(err_reply));
        free(name);
        free(cwd);
        rpc_free_strv(argv, argc);
        rpc_free_strv(envp, envc);
        return;
    }
    pthread_mutex_init(&inv->rt_lock, NULL);
    inv->rt = rt;
    inv->command_release = rt->callbacks.command_release;
    inv->inv_id = inv_id;
    atomic_init(&inv->refcount, 2);
    pthread_mutex_init(&inv->out_flush.lock, NULL);
    pthread_cond_init(&inv->out_flush.cond, NULL);
    inv->out_flush.remaining = 2;

    if (!register_stream_route(rt, SWIPTERMD_STREAM_CMD_IN, stream_token, sp_in[1], STREAM_DIR_GUEST_TO_HOST, NULL)) {
        close(sp_in[1]);
    }
    if (!register_stream_route(rt, SWIPTERMD_STREAM_CMD_OUT, stream_token, sp_out[1], STREAM_DIR_HOST_TO_GUEST, inv)) {
        close(sp_out[1]);
        pthread_mutex_lock(&inv->out_flush.lock);
        inv->out_flush.remaining--;
        pthread_mutex_unlock(&inv->out_flush.lock);
    }
    if (!register_stream_route(rt, SWIPTERMD_STREAM_CMD_ERR, stream_token, sp_err[1], STREAM_DIR_HOST_TO_GUEST, inv)) {
        close(sp_err[1]);
        pthread_mutex_lock(&inv->out_flush.lock);
        inv->out_flush.remaining--;
        pthread_mutex_unlock(&inv->out_flush.lock);
    }

    pthread_mutex_lock(&rt->state_lock);
    bool stopping = atomic_load(&rt->shutting_down);
    command = stopping ? NULL : command_find(rt, name);
    if (!command) {
        pthread_mutex_unlock(&rt->state_lock);
        close(sp_in[0]);
        close(sp_out[0]);
        close(sp_err[0]);
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_IN, stream_token);
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_OUT, stream_token);
        cancel_stream_route(rt, SWIPTERMD_STREAM_CMD_ERR, stream_token);
        invocation_release_internal(inv);
        invocation_release_internal(inv);
        if (!stopping) {
            swiptermd_enc_err(err_reply, -LINUX_ENOENT);
            control_send(rt, SWIPTERMD_OP_CMD_START | SWIPTERMD_OP_REPLY_FLAG, req_id, err_reply, sizeof(err_reply));
        }
        free(name);
        free(cwd);
        rpc_free_strv(argv, argc);
        rpc_free_strv(envp, envc);
        return;
    }
    command->users++;
    inv->next = rt->invocations;
    rt->invocations = inv;
    pthread_mutex_unlock(&rt->state_lock);

    void *swift_ctx = rt->callbacks.command_start(
        command->user_context,
        inv,
        argc,
        (const char *const *)argv,
        (const char *const *)envp,
        cwd,
        sp_in[0],
        sp_out[0],
        sp_err[0]
    );

    bool already_cancelled = false;
    pthread_mutex_lock(&rt->state_lock);
    registration_unuse(rt, &command->users);
    inv->swift_context = swift_ctx;
    already_cancelled = inv->cancelled || atomic_load(&rt->shutting_down);
    if (already_cancelled) {
        inv->cancelled = true;
    }
    pthread_mutex_unlock(&rt->state_lock);

    swiptermd_enc_err(err_reply, 0);
    control_send(rt, SWIPTERMD_OP_CMD_START | SWIPTERMD_OP_REPLY_FLAG, req_id, err_reply, sizeof(err_reply));

    if (already_cancelled && swift_ctx && rt->callbacks.command_abort) {
        rt->callbacks.command_abort(swift_ctx);
    }

    free(name);
    free(cwd);
    rpc_free_strv(argv, argc);
    rpc_free_strv(envp, envc);
}

static void handle_dev_rpc(swipterm_runtime *rt, uint16_t opcode, uint32_t req_id,
                           const uint8_t *body, uint32_t len) {
    uint16_t reply_op = (uint16_t)(opcode | SWIPTERMD_OP_REPLY_FLAG);

    switch ((swiptermd_opcode_t)opcode) {
    case SWIPTERMD_OP_DEV_OPEN: {
        uint8_t reply[SWIPTERMD_DEV_OPEN_REPLY_SIZE];
        uint32_t dev_id = 0;
        uint32_t flags = 0;
        if (!swiptermd_dec_dev_open_req(body, len, &dev_id, &flags) || !rt->callbacks.device_open) {
            swiptermd_enc_dev_open_reply(reply, -LINUX_EINVAL, 0);
            control_send(rt, reply_op, req_id, reply, sizeof(reply));
            return;
        }
        device_entry_t *device = NULL;
        pthread_mutex_lock(&rt->state_lock);
        for (device_entry_t *d = rt->devices; d != NULL; d = d->next) {
            if (d->dev_id == dev_id) {
                device = d;
                device->users++;
                break;
            }
        }
        pthread_mutex_unlock(&rt->state_lock);
        if (!device) {
            swiptermd_enc_dev_open_reply(reply, -LINUX_ENODEV, 0);
            control_send(rt, reply_op, req_id, reply, sizeof(reply));
            return;
        }
        int err = 0;
        void *file_ptr = rt->callbacks.device_open(device->user_context, (int)flags, &err);
        pthread_mutex_lock(&rt->state_lock);
        registration_unuse(rt, &device->users);
        pthread_mutex_unlock(&rt->state_lock);
        if (!file_ptr) {
            swiptermd_enc_dev_open_reply(reply, err < 0 ? err : -LINUX_EIO, 0);
            control_send(rt, reply_op, req_id, reply, sizeof(reply));
            return;
        }
        device_file_entry_t *fe = calloc(1, sizeof(*fe));
        if (!fe) {
            if (rt->callbacks.device_close) {
                rt->callbacks.device_close(file_ptr);
            }
            swiptermd_enc_dev_open_reply(reply, -LINUX_ENOMEM, 0);
            control_send(rt, reply_op, req_id, reply, sizeof(reply));
            return;
        }
        fe->dev_id = dev_id;
        fe->file_id = atomic_fetch_add(&rt->next_file_id, 1) + 1;
        fe->file_ptr = file_ptr;
        fe->device_close = rt->callbacks.device_close;
        atomic_init(&fe->refcount, 1);
        pthread_mutex_lock(&rt->state_lock);
        if (atomic_load(&rt->shutting_down)) {
            pthread_mutex_unlock(&rt->state_lock);
            device_file_release(fe);
            return;
        }
        fe->next = rt->device_files;
        rt->device_files = fe;
        pthread_mutex_unlock(&rt->state_lock);
        swiptermd_enc_dev_open_reply(reply, 0, fe->file_id);
        control_send(rt, reply_op, req_id, reply, sizeof(reply));
        return;
    }
    case SWIPTERMD_OP_DEV_READ: {
        uint8_t err_reply[SWIPTERMD_ERR_REPLY_SIZE];
        uint32_t dev_id = 0;
        uint64_t file_id = 0;
        uint32_t max_len = 0;
        if (!swiptermd_dec_dev_read_req(body, len, &dev_id, &file_id, &max_len) ||
            !rt->callbacks.device_read) {
            swiptermd_enc_err(err_reply, -LINUX_EINVAL);
            control_send(rt, reply_op, req_id, err_reply, sizeof(err_reply));
            return;
        }
        device_file_entry_t *matched_fe = NULL;
        pthread_mutex_lock(&rt->state_lock);
        for (device_file_entry_t *fe = rt->device_files; fe != NULL; fe = fe->next) {
            if (fe->file_id == file_id && fe->dev_id == dev_id) {
                atomic_fetch_add(&fe->refcount, 1);
                matched_fe = fe;
                break;
            }
        }
        pthread_mutex_unlock(&rt->state_lock);
        if (!matched_fe) {
            swiptermd_enc_err(err_reply, -LINUX_EBADF);
            control_send(rt, reply_op, req_id, err_reply, sizeof(err_reply));
            return;
        }
        if (max_len > MAX_DEV_READ_CHUNK) {
            max_len = MAX_DEV_READ_CHUNK;
        }
        uint8_t *buf = malloc(max_len ? max_len : 1);
        if (!buf) {
            device_file_release(matched_fe);
            swiptermd_enc_err(err_reply, -LINUX_ENOMEM);
            control_send(rt, reply_op, req_id, err_reply, sizeof(err_reply));
            return;
        }
        ssize_t n = rt->callbacks.device_read(matched_fe->file_ptr, buf, max_len);
        device_file_release(matched_fe);
        if (n < 0) {
            free(buf);
            swiptermd_enc_err(err_reply, (int32_t)n);
            control_send(rt, reply_op, req_id, err_reply, sizeof(err_reply));
            return;
        }
        size_t rcap = 8 + (size_t)n;
        uint8_t *reply = malloc(rcap);
        if (!reply) {
            free(buf);
            swiptermd_enc_err(err_reply, -LINUX_ENOMEM);
            control_send(rt, reply_op, req_id, err_reply, sizeof(err_reply));
            return;
        }
        swiptermd_buf_w_t w = swiptermd_buf_w_init(reply, rcap);
        swiptermd_enc_dev_read_reply(&w, 0, buf, (uint32_t)n);
        free(buf);
        control_send(rt, reply_op, req_id, reply, (uint32_t)w.len);
        free(reply);
        return;
    }
    case SWIPTERMD_OP_DEV_WRITE: {
        uint8_t reply[SWIPTERMD_DEV_WRITE_REPLY_SIZE];
        uint32_t dev_id = 0;
        uint64_t file_id = 0;
        swiptermd_slice_t data = {NULL, 0};
        if (!swiptermd_dec_dev_write_req(body, len, &dev_id, &file_id, &data) ||
            !rt->callbacks.device_write) {
            swiptermd_enc_dev_write_reply(reply, -LINUX_EINVAL, 0);
            control_send(rt, reply_op, req_id, reply, sizeof(reply));
            return;
        }
        device_file_entry_t *matched_fe = NULL;
        pthread_mutex_lock(&rt->state_lock);
        for (device_file_entry_t *fe = rt->device_files; fe != NULL; fe = fe->next) {
            if (fe->file_id == file_id && fe->dev_id == dev_id) {
                atomic_fetch_add(&fe->refcount, 1);
                matched_fe = fe;
                break;
            }
        }
        pthread_mutex_unlock(&rt->state_lock);
        if (!matched_fe) {
            swiptermd_enc_dev_write_reply(reply, -LINUX_EBADF, 0);
            control_send(rt, reply_op, req_id, reply, sizeof(reply));
            return;
        }
        uint8_t empty = 0;
        ssize_t n = rt->callbacks.device_write(matched_fe->file_ptr, data.ptr ? data.ptr : &empty, data.len);
        device_file_release(matched_fe);
        if (n < 0) {
            swiptermd_enc_dev_write_reply(reply, (int32_t)n, 0);
        } else {
            swiptermd_enc_dev_write_reply(reply, 0, (uint32_t)n);
        }
        control_send(rt, reply_op, req_id, reply, sizeof(reply));
        return;
    }
    case SWIPTERMD_OP_DEV_CLOSE: {
        uint8_t reply[SWIPTERMD_ERR_REPLY_SIZE];
        uint32_t dev_id = 0;
        uint64_t file_id = 0;
        if (!swiptermd_dec_dev_close_req(body, len, &dev_id, &file_id)) {
            swiptermd_enc_err(reply, -LINUX_EINVAL);
            control_send(rt, reply_op, req_id, reply, sizeof(reply));
            return;
        }
        device_file_entry_t *matched_fe = NULL;
        pthread_mutex_lock(&rt->state_lock);
        for (device_file_entry_t **pp = &rt->device_files; *pp; pp = &(*pp)->next) {
            if ((*pp)->file_id == file_id && (*pp)->dev_id == dev_id) {
                matched_fe = *pp;
                *pp = matched_fe->next;
                break;
            }
        }
        pthread_mutex_unlock(&rt->state_lock);
        if (matched_fe) {
            device_file_release(matched_fe);
        }
        swiptermd_enc_err(reply, 0);
        control_send(rt, reply_op, req_id, reply, sizeof(reply));
        return;
    }
    default: {
        uint8_t reply[SWIPTERMD_ERR_REPLY_SIZE];
        swiptermd_enc_err(reply, -LINUX_ENOSYS);
        control_send(rt, reply_op, req_id, reply, sizeof(reply));
        return;
    }
    }
}

static void *guest_rpc_worker(void *arg) {
    guest_rpc_task_t *task = arg;
    swipterm_runtime *rt = task->rt;
    uint16_t op = task->hdr.opcode;
    uint32_t req_id = task->hdr.req_id;
    uint8_t *body = task->body;
    uint32_t len = task->hdr.body_len;
    free(task);

    switch ((swiptermd_opcode_t)op) {
    case SWIPTERMD_OP_CMD_START:
        handle_cmd_start(rt, req_id, body, len);
        break;
    case SWIPTERMD_OP_DEV_OPEN:
    case SWIPTERMD_OP_DEV_READ:
    case SWIPTERMD_OP_DEV_WRITE:
    case SWIPTERMD_OP_DEV_CLOSE:
        handle_dev_rpc(rt, op, req_id, body, len);
        break;
    case SWIPTERMD_OP_HALT: {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        pthread_mutex_lock(&rt->state_lock);
        while (rt->console_active && !atomic_load(&rt->shutting_down)) {
            if (pthread_cond_timedwait(&rt->console_cond, &rt->state_lock, &ts) == ETIMEDOUT) {
                break;
            }
        }
        swipterm_halt_info info = rt->halt_info;
        pthread_mutex_unlock(&rt->state_lock);
        free(body);
        if (!atomic_exchange(&rt->halted, true) && !atomic_load(&rt->shutting_down) && rt->callbacks.halt) {
            rt->callbacks.halt(rt->callbacks.context, info.reason, info.wait_status);
        }
        pthread_mutex_lock(&rt->state_lock);
        pthread_cond_broadcast(&rt->ready_cond);
        pthread_mutex_unlock(&rt->state_lock);
        worker_leave(rt);
        return NULL;
    }
    default:
        break;
    }

    free(body);
    worker_leave(rt);
    return NULL;
}

static void spawn_guest_rpc_worker(swipterm_runtime *rt, const swiptermd_frame_hdr_t *hdr, uint8_t *body) {
    if (!worker_try_enter(rt)) {
        free(body);
        return;
    }
    guest_rpc_task_t *task = malloc(sizeof(*task));
    if (!task) {
        free(body);
        worker_leave(rt);
        return;
    }
    task->rt = rt;
    task->hdr = *hdr;
    task->body = body;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t tid;
    if (pthread_create(&tid, &attr, guest_rpc_worker, task) != 0) {
        free(task);
        free(body);
        worker_leave(rt);
    }
    pthread_attr_destroy(&attr);
}

static void handle_guest_frame(swipterm_runtime *rt, const swiptermd_frame_hdr_t *hdr, uint8_t *body) {
    if (hdr->opcode & SWIPTERMD_OP_REPLY_FLAG) {
        pending_req_t *matched = NULL;
        pthread_mutex_lock(&rt->state_lock);
        for (pending_req_t **pp = &rt->pending_reqs; *pp; pp = &(*pp)->next) {
            if ((*pp)->req_id == hdr->req_id) {
                matched = *pp;
                *pp = matched->next;
                break;
            }
        }
        pthread_mutex_unlock(&rt->state_lock);
        if (matched) {
            pthread_mutex_lock(&matched->lock);
            matched->reply_body = body;
            matched->reply_len = hdr->body_len;
            matched->done = true;
            pthread_cond_signal(&matched->cond);
            pthread_mutex_unlock(&matched->lock);
        } else {
            free(body);
        }
        return;
    }

    switch ((swiptermd_opcode_t)hdr->opcode) {
    case SWIPTERMD_OP_HELLO: {
        uint64_t nonce = 0;
        uint32_t caps = 0;
        bool ok = swiptermd_dec_hello(body, hdr->body_len, &nonce, &caps);
        free(body);

        uint8_t reply[SWIPTERMD_ERR_REPLY_SIZE];
        if (!ok) {
            swiptermd_enc_err(reply, -LINUX_EINVAL);
            control_send(rt, SWIPTERMD_OP_HELLO | SWIPTERMD_OP_REPLY_FLAG, hdr->req_id, reply, sizeof(reply));
            break;
        }

        pthread_mutex_lock(&rt->state_lock);
        rt->session_nonce = nonce;
        rt->guest_caps = caps;
        rt->hello_done = true;
        pthread_cond_broadcast(&rt->hello_cond);
        pthread_mutex_unlock(&rt->state_lock);
        swipterm_network_set_session(rt->network, nonce);

        swiptermd_enc_err(reply, 0);
        control_send(rt, SWIPTERMD_OP_HELLO | SWIPTERMD_OP_REPLY_FLAG, hdr->req_id, reply, sizeof(reply));
        break;
    }
    case SWIPTERMD_OP_READY:
    case SWIPTERMD_OP_STOPPED: {
        uint32_t value = 0;
        bool ok = swiptermd_dec_u32_notify(body, hdr->body_len, &value);
        free(body);
        if (!ok) {
            break;
        }
        pthread_mutex_lock(&rt->state_lock);
        if (hdr->opcode == SWIPTERMD_OP_READY) {
            if (value < 32) {
                rt->ready_mask |= 1U << value;
            }
        } else {
            rt->halt_info.shutdown_complete = true;
            rt->halt_info.forced = (value & SWIPTERMD_STOP_FORCED) != 0;
            rt->halt_info.storage_error = (value & SWIPTERMD_STOP_STORAGE_ERROR) != 0;
        }
        pthread_cond_broadcast(&rt->ready_cond);
        pthread_mutex_unlock(&rt->state_lock);
        break;
    }
    case SWIPTERMD_OP_PROCESS_EXIT: {
        uint32_t proc_id = 0;
        int32_t pid = 0;
        int32_t wait_status = 0;
        bool ok = swiptermd_dec_process_exit(body, hdr->body_len, &proc_id, &pid, &wait_status);
        free(body);
        (void)pid;
        if (!ok) {
            break;
        }
        swipterm_process *proc = NULL;
        swipterm_terminal *term = NULL;
        pthread_mutex_lock(&rt->state_lock);
        for (swipterm_process **pp = &rt->processes; *pp; pp = &(*pp)->next) {
            if ((*pp)->proc_id == proc_id) {
                proc = *pp;
                *pp = proc->next;
                atomic_store(&proc->exited, true);
                term = proc->terminal;
                proc->terminal = NULL;
                break;
            }
        }
        pthread_mutex_unlock(&rt->state_lock);
        if (proc) {
            pthread_mutex_lock(&proc->rt_lock);
            proc->rt = NULL;
            pthread_mutex_unlock(&proc->rt_lock);
            if (term) {
                pthread_mutex_lock(&term->lock);
                term->in_use = false;
                pthread_mutex_unlock(&term->lock);
                terminal_release_internal(term);
            }
            if (rt->callbacks.process_exit) {
                rt->callbacks.process_exit(proc->user_context, wait_status);
            }
            process_release_internal(proc);
        }
        break;
    }
    case SWIPTERMD_OP_CMD_CANCEL: {
        uint32_t inv_id = 0;
        int32_t sig = 0;
        bool ok = swiptermd_dec_cmd_cancel(body, hdr->body_len, &inv_id, &sig);
        free(body);
        (void)sig;
        if (!ok) {
            break;
        }
        swipterm_invocation *inv = NULL;
        void *swift_ctx = NULL;
        pthread_mutex_lock(&rt->state_lock);
        for (swipterm_invocation *cur = rt->invocations; cur != NULL; cur = cur->next) {
            if (cur->inv_id == inv_id) {
                inv = cur;
                inv->cancelled = true;
                swift_ctx = inv->swift_context;
                atomic_fetch_add(&inv->refcount, 1);
                break;
            }
        }
        pthread_mutex_unlock(&rt->state_lock);
        if (inv) {
            if (swift_ctx && rt->callbacks.command_abort) {
                rt->callbacks.command_abort(swift_ctx);
            }
            invocation_release_internal(inv);
        }
        break;
    }
    case SWIPTERMD_OP_MIRROR_UPDATE: {
        swiptermd_buf_r_t r = swiptermd_buf_r_init(body, hdr->body_len);
        uint32_t count = swiptermd_r_u32(&r);
        if (!r.underflow && count > 0 && count <= (r.len - r.pos) / SWIPTERMD_MIRROR_ENTRY_WIRE_SIZE) {
            swiptermd_mirror_entry_t *entries = calloc(count, sizeof(*entries));
            if (entries) {
                for (uint32_t i = 0; i < count; i++) {
                    swiptermd_mirror_entry_decode(&r, &entries[i]);
                }
                if (!r.underflow) {
                    swipterm_network_push_listeners(rt->network, entries, count);
                }
                free(entries);
            }
        }
        free(body);
        break;
    }
    case SWIPTERMD_OP_HALT: {
        uint32_t reason = 0;
        int32_t status = 0;
        if (swiptermd_dec_halt_notify(body, hdr->body_len, &reason, &status)) {
            pthread_mutex_lock(&rt->state_lock);
            if (rt->halt_info.reason == SWIPTERM_HALT_NONE) {
                rt->halt_info.reason = reason == SWIPTERMD_HALT_REASON_REBOOT     ? SWIPTERM_HALT_REBOOT
                                       : reason == SWIPTERMD_HALT_REASON_POWEROFF ? SWIPTERM_HALT_POWER_OFF
                                                                                  : SWIPTERM_HALT_EXIT;
                rt->halt_info.wait_status = status;
            }
            pthread_cond_broadcast(&rt->ready_cond);
            pthread_mutex_unlock(&rt->state_lock);
        }
        spawn_guest_rpc_worker(rt, hdr, body);
        break;
    }
    case SWIPTERMD_OP_CMD_START:
    case SWIPTERMD_OP_DEV_OPEN:
    case SWIPTERMD_OP_DEV_READ:
    case SWIPTERMD_OP_DEV_WRITE:
    case SWIPTERMD_OP_DEV_CLOSE:
        spawn_guest_rpc_worker(rt, hdr, body);
        break;
    default:
        free(body);
        break;
    }
}

static void fail_all_pending_reqs(swipterm_runtime *rt) {
    pthread_mutex_lock(&rt->state_lock);
    pending_req_t *list = rt->pending_reqs;
    rt->pending_reqs = NULL;
    pthread_cond_broadcast(&rt->hello_cond);
    pthread_cond_broadcast(&rt->ready_cond);
    pthread_mutex_unlock(&rt->state_lock);

    while (list) {
        pending_req_t *next = list->next;
        pthread_mutex_lock(&list->lock);
        list->failed = true;
        list->done = true;
        pthread_cond_signal(&list->cond);
        pthread_mutex_unlock(&list->lock);
        list = next;
    }
}

static void *control_rx_main(void *arg) {
    swipterm_runtime *rt = arg;
    for (;;) {
        swiptermd_frame_hdr_t hdr;
        uint8_t *body = NULL;
        if (!rpc_recv_frame(rt->control_fd, &hdr, &body)) {
            break;
        }
        handle_guest_frame(rt, &hdr, body);
    }
    pthread_mutex_lock(&rt->state_lock);
    rt->control_rx_done = true;
    pthread_mutex_unlock(&rt->state_lock);
    fail_all_pending_reqs(rt);
    return NULL;
}

static void vmm_on_console(void *ctx, const void *data, size_t len) {
    swipterm_runtime *rt = ctx;
    if (rt->console_log) {
        (void)fwrite(data, 1, len, stderr);
        (void)fflush(stderr);
    }
}

static void vmm_on_halt(void *ctx) {
    swipterm_runtime *rt = ctx;
    atomic_store(&rt->vm_powered_off, true);
    bool first = !atomic_exchange(&rt->halted, true);
    pthread_mutex_lock(&rt->state_lock);
    pthread_cond_broadcast(&rt->halt_cond);
    pthread_mutex_unlock(&rt->state_lock);
    fail_all_pending_reqs(rt);
    if (rt->vsock) {
        virtio_vsock_notify_halt(rt->vsock);
    }
    int control_fd = rt->control_fd;
    if (control_fd >= 0) {
        (void)shutdown(control_fd, SHUT_RDWR);
    }
    if (first && !atomic_load(&rt->shutting_down) && !atomic_load(&rt->booting) && rt->callbacks.halt) {
        pthread_mutex_lock(&rt->state_lock);
        swipterm_halt_info info = rt->halt_info;
        pthread_mutex_unlock(&rt->state_lock);
        rt->callbacks.halt(rt->callbacks.context, info.reason, info.wait_status);
    }
}

static void vmm_on_panic(void *ctx, const char *message) {
    swipterm_runtime *rt = ctx;
    if (!atomic_load(&rt->booting) && rt->callbacks.panic) {
        rt->callbacks.panic(rt->callbacks.context, message);
    }
}

static size_t default_ram_size(void) {
    const uint64_t step = 16ULL << 20;
    const uint64_t min_size = 256ULL << 20;
    uint64_t memsize = 0;
    size_t len = sizeof(memsize);
    (void)sysctlbyname("hw.memsize", &memsize, &len, NULL, 0);
    uint64_t target = (memsize / 2) & ~(step - 1);
    return (size_t)(target < min_size ? min_size : target);
}

static uint32_t default_cpu_count(void) {
    int ncpu = 1;
    size_t len = sizeof(ncpu);
    if (sysctlbyname("hw.activecpu", &ncpu, &len, NULL, 0) != 0 || ncpu < 1) {
        ncpu = 1;
    }
    return ncpu < VMM_MAX_VCPUS ? (uint32_t)ncpu : VMM_MAX_VCPUS;
}

static pthread_once_t g_open_file_limit_once = PTHREAD_ONCE_INIT;

static void raise_open_file_limit(void) {
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) != 0) {
        return;
    }
    rlim_t target = limit.rlim_max < OPEN_MAX ? limit.rlim_max : OPEN_MAX;
    if (limit.rlim_cur < target) {
        limit.rlim_cur = target;
        (void)setrlimit(RLIMIT_NOFILE, &limit);
    }
}

typedef struct initramfs_cache {
    char *path;
    dev_t dev;
    ino_t ino;
    off_t size;
    struct timespec mtime;
    uint8_t digest[CC_SHA256_DIGEST_LENGTH];
    uint8_t *data;
    size_t data_size;
} initramfs_cache_t;

static pthread_mutex_t g_initramfs_lock = PTHREAD_MUTEX_INITIALIZER;
static initramfs_cache_t g_initramfs;

static bool initramfs_cache_matches(const char *path, const struct stat *st) {
    return g_initramfs.path && strcmp(g_initramfs.path, path) == 0 && g_initramfs.dev == st->st_dev &&
           g_initramfs.ino == st->st_ino && g_initramfs.size == st->st_size &&
           g_initramfs.mtime.tv_sec == st->st_mtimespec.tv_sec &&
           g_initramfs.mtime.tv_nsec == st->st_mtimespec.tv_nsec;
}

static int initramfs_refresh(const char *path, const struct stat *st) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return errno == ENOENT ? -LINUX_ENOENT : -LINUX_EIO;
    }
    size_t size = (size_t)st->st_size;
    uint8_t *file = malloc(size > 0 ? size : 1);
    size_t done = 0;
    while (file && done < size) {
        ssize_t n = read(fd, file + done, size - done);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            break;
        }
        done += (size_t)n;
    }
    close(fd);
    if (!file || done != size) {
        free(file);
        return file ? -LINUX_EIO : -LINUX_ENOMEM;
    }
    initramfs_cache_t next = {
        .dev = st->st_dev,
        .ino = st->st_ino,
        .size = st->st_size,
        .mtime = st->st_mtimespec,
    };
    CC_SHA256(file, (CC_LONG)size, next.digest);
    int rc = swipterm_decompress(file, size, 512U << 20, &next.data, &next.data_size);
    free(file);
    next.path = rc == 0 ? strdup(path) : NULL;
    if (rc < 0 || !next.path) {
        free(next.data);
        return rc == -ENOMEM || rc == 0 ? -LINUX_ENOMEM : -LINUX_EIO;
    }
    free(g_initramfs.path);
    free(g_initramfs.data);
    g_initramfs = next;
    return 0;
}

static int initramfs_get(const char *path, uint8_t digest[CC_SHA256_DIGEST_LENGTH],
                         const uint8_t *modules, size_t modules_size, uint8_t **initrd_out, size_t *initrd_size_out) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return errno == ENOENT ? -LINUX_ENOENT : -LINUX_EIO;
    }
    pthread_mutex_lock(&g_initramfs_lock);
    int rc = initramfs_cache_matches(path, &st) ? 0 : initramfs_refresh(path, &st);
    if (rc == 0) {
        memcpy(digest, g_initramfs.digest, CC_SHA256_DIGEST_LENGTH);
    }
    if (rc == 0 && initrd_out) {
        size_t aligned = (g_initramfs.data_size + 3U) & ~(size_t)3U;
        uint8_t *initrd = calloc(1, aligned + modules_size);
        if (initrd) {
            memcpy(initrd, g_initramfs.data, g_initramfs.data_size);
            if (modules_size > 0) {
                memcpy(initrd + aligned, modules, modules_size);
            }
            *initrd_out = initrd;
            *initrd_size_out = aligned + modules_size;
        } else {
            rc = -LINUX_ENOMEM;
        }
    }
    pthread_mutex_unlock(&g_initramfs_lock);
    return rc;
}

static void compute_snapshot_key(const uint8_t initramfs_digest[CC_SHA256_DIGEST_LENGTH], const boot_plan_t *plan,
                                 const char *cmdline, uint8_t key[CC_SHA256_DIGEST_LENGTH]) {
    static const char tag[] = "swipterm-boot-4";
    CC_SHA256_CTX ctx;
    CC_SHA256_Init(&ctx);
    CC_SHA256_Update(&ctx, tag, sizeof(tag));
    CC_SHA256_Update(&ctx, initramfs_digest, CC_SHA256_DIGEST_LENGTH);
    CC_SHA256_Update(&ctx, boot_plan_identity(plan), CC_SHA256_DIGEST_LENGTH);
    CC_SHA256_Update(&ctx, cmdline, (CC_LONG)strlen(cmdline));
    CC_SHA256_Final(key, &ctx);
}

static int host_error(int rc) {
    switch (rc) {
    case -ENOEXEC:
        return -LINUX_ENOEXEC;
    case -ENOMEM:
        return -LINUX_ENOMEM;
    case -ENOENT:
        return -LINUX_ENOENT;
    case -EINVAL:
        return -LINUX_EINVAL;
    default:
        return -LINUX_EIO;
    }
}

static int start_machine(swipterm_runtime *rt, const char *save_path) {
    rt->vsock = rt->vm->virtio_devs[VMM_VIRTIO_SLOT_VSOCK];
    if (!rt->vsock) {
        return -LINUX_ENODEV;
    }
    if (virtio_vsock_listen_host(rt->vsock, SWIPTERMD_CONTROL_PORT, NULL, NULL) != 0 ||
        virtio_vsock_listen_host(rt->vsock, SWIPTERMD_DATA_PORT, on_data_stream_accept, rt) != 0) {
        return -LINUX_EIO;
    }
    swipterm_network_set_runtime(rt->network, rt->vsock);
    if (vmm_start(rt->vm) < 0) {
        return -LINUX_EIO;
    }
    int save_error = 0;
    if (vmm_finish_preboot(rt->vm, save_path, &save_error) < 0) {
        return -LINUX_EIO;
    }
    return 0;
}

static int connect_guest(swipterm_runtime *rt) {
    rt->control_fd = virtio_vsock_accept_control(rt->vsock, SWIPTERMD_CONTROL_PORT, CONTROL_BOOT_TIMEOUT_MS);
    if (rt->control_fd < 0) {
        return atomic_load(&rt->halted) ? -LINUX_EIO : -LINUX_ETIMEDOUT;
    }
    set_nosigpipe(rt->control_fd);
    set_cloexec(rt->control_fd);
    if (pthread_create(&rt->control_rx_thread, NULL, control_rx_main, rt) != 0) {
        return -LINUX_EAGAIN;
    }
    rt->control_rx_started = true;

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += CONTROL_BOOT_TIMEOUT_MS / 1000;
    pthread_mutex_lock(&rt->state_lock);
    while (!rt->hello_done && !rt->control_rx_done && !atomic_load(&rt->shutting_down) &&
           !atomic_load(&rt->halted)) {
        if (pthread_cond_timedwait(&rt->hello_cond, &rt->state_lock, &deadline) == ETIMEDOUT) {
            break;
        }
    }
    bool hello_ok = rt->hello_done;
    bool stopped = rt->control_rx_done || atomic_load(&rt->halted);
    pthread_mutex_unlock(&rt->state_lock);
    if (hello_ok) {
        return 0;
    }
    return stopped ? -LINUX_EIO : -LINUX_ETIMEDOUT;
}

static void discard_machine(swipterm_runtime *rt) {
    pthread_mutex_lock(&rt->control_tx_lock);
    if (rt->control_fd >= 0) {
        (void)shutdown(rt->control_fd, SHUT_RDWR);
    }
    pthread_mutex_unlock(&rt->control_tx_lock);
    if (rt->control_rx_started) {
        pthread_join(rt->control_rx_thread, NULL);
        rt->control_rx_started = false;
    }
    if (rt->vsock) {
        virtio_vsock_notify_halt(rt->vsock);
    }
    swipterm_network_set_runtime(rt->network, NULL);
    if (rt->vm) {
        vmm_stop(rt->vm);
        vmm_destroy(rt->vm);
        rt->vm = NULL;
    }
    pthread_mutex_lock(&rt->control_tx_lock);
    if (rt->control_fd >= 0) {
        close(rt->control_fd);
        rt->control_fd = -1;
    }
    pthread_mutex_unlock(&rt->control_tx_lock);
    rt->vsock = NULL;
    atomic_store(&rt->halted, false);
    atomic_store(&rt->vm_powered_off, false);
    pthread_mutex_lock(&rt->state_lock);
    rt->hello_done = false;
    rt->control_rx_done = false;
    rt->session_nonce = 0;
    rt->guest_caps = 0;
    rt->ready_mask = 0;
    rt->halt_info = (swipterm_halt_info){0};
    pthread_mutex_unlock(&rt->state_lock);
}

static const char *boot_snapshot_path(const swipterm_options *options, char *buf, size_t size) {
    const char *path = getenv("SWIPTERM_BOOT_SNAPSHOT");
    if (path) {
        return path[0] == '\0' || strcmp(path, "0") == 0 || strcmp(path, "off") == 0 ? NULL : path;
    }
    return snprintf(buf, size, "%s/.preboot_snapshot", options->root_path) < (int)size ? buf : NULL;
}

static int boot_machine(swipterm_runtime *rt, const swipterm_options *options, boot_plan_t *plan,
                        const char *disk_path, const char *cmdline) {
    uint8_t initramfs_digest[CC_SHA256_DIGEST_LENGTH];
    int rc = initramfs_get(options->initramfs_path, initramfs_digest, NULL, 0, NULL, NULL);
    if (rc < 0) {
        return rc;
    }
    const vmm_config_t base = {
        .ram_size = options->ram_size ? options->ram_size : default_ram_size(),
        .cpu_count = options->cpu_count ? options->cpu_count : default_cpu_count(),
        .root_disk_path = disk_path,
        .preboot_checkpoint = true,
        .cmdline = cmdline,
        .network = rt->network,
        .callback_ctx = rt,
        .console_output = vmm_on_console,
        .on_halt = vmm_on_halt,
        .on_panic = vmm_on_panic,
    };
    bool save_snapshots = getenv("SWIPTERM_BOOT_SNAPSHOT_READONLY") == NULL;
    bool restore_snapshot = true;
    atomic_store(&rt->booting, true);
    for (;;) {
        char snapshot_buf[1024];
        const char *snapshot_path = boot_snapshot_path(options, snapshot_buf, sizeof(snapshot_buf));
        const char *save_path = save_snapshots ? snapshot_path : NULL;
        vmm_config_t cfg = base;
        int err = 0;
        bool restored = false;
        if (snapshot_path && restore_snapshot) {
            compute_snapshot_key(initramfs_digest, plan, cmdline, cfg.snapshot_key);
            cfg.snapshot_path = snapshot_path;
            cfg.snapshot_required = true;
            rt->vm = vmm_create(&cfg, &err);
            if (!rt->vm && err == EWOULDBLOCK) {
                return -LINUX_EBUSY;
            }
            restored = rt->vm != NULL;
        }

        if (!restored) {
            uint8_t *kernel = NULL;
            uint8_t *modules = NULL;
            size_t kernel_size = 0;
            size_t modules_size = 0;
            rc = boot_plan_load(plan, &kernel, &kernel_size, &modules, &modules_size);
            if (rc == -EAGAIN) {
                restore_snapshot = true;
                continue;
            }
            if (rc < 0) {
                return host_error(rc);
            }
            uint8_t *initrd = NULL;
            size_t initrd_size = 0;
            rc = initramfs_get(options->initramfs_path, initramfs_digest, modules, modules_size, &initrd,
                               &initrd_size);
            free(modules);
            if (rc < 0) {
                free(kernel);
                return rc;
            }
            compute_snapshot_key(initramfs_digest, plan, cmdline, cfg.snapshot_key);
            cfg.snapshot_path = NULL;
            cfg.snapshot_required = false;
            cfg.kernel_image = kernel;
            cfg.kernel_size = kernel_size;
            cfg.initramfs = initrd;
            cfg.initramfs_size = initrd_size;
            rt->vm = vmm_create(&cfg, &err);
            free(kernel);
            free(initrd);
            if (!rt->vm) {
                if (err == EWOULDBLOCK || err == ENOMEM || err == EINVAL || atomic_load(&rt->shutting_down)) {
                    return err == EWOULDBLOCK ? -LINUX_EBUSY
                           : err == ENOMEM    ? -LINUX_ENOMEM
                           : err == EINVAL    ? -LINUX_EINVAL
                                              : -LINUX_EIO;
                }
                rc = boot_plan_next(plan, "could not be loaded into guest memory");
                if (rc < 0) {
                    return host_error(rc);
                }
                restore_snapshot = true;
                continue;
            }
        }

        rc = start_machine(rt, restored ? NULL : save_path);
        bool reached_checkpoint = rc == 0;
        if (reached_checkpoint) {
            rc = connect_guest(rt);
        }
        if (rc == 0) {
            atomic_store(&rt->booting, false);
            return 0;
        }
        bool guest_failed = reached_checkpoint ? rc == -LINUX_EIO || rc == -LINUX_ETIMEDOUT : !restored;
        discard_machine(rt);
        if (!guest_failed || atomic_load(&rt->shutting_down)) {
            return rc;
        }
        if (reached_checkpoint && save_path) {
            (void)unlink(save_path);
        }
        if (restored) {
            restore_snapshot = false;
            continue;
        }
        const char *reason = !reached_checkpoint ? "did not reach the boot checkpoint"
                             : rc == -LINUX_EIO  ? "stopped during initialization"
                                                 : "timed out during initialization";
        rc = boot_plan_next(plan, reason);
        if (rc < 0) {
            return host_error(rc);
        }
        restore_snapshot = true;
    }
}

int swipterm_boot(const swipterm_options *options, swipterm_runtime **runtime_out, char **error_message) {
    if (error_message) {
        *error_message = NULL;
    }
    if (!options || !options->root_path || !options->initramfs_path || !options->network || !runtime_out ||
        (options->kernel != SWIPTERM_KERNEL_INSTALLED && options->kernel != SWIPTERM_KERNEL_RELEASE) ||
        (options->kernel == SWIPTERM_KERNEL_RELEASE && (!options->kernel_release || !options->kernel_release[0]))) {
        return -LINUX_EINVAL;
    }
    *runtime_out = NULL;
    pthread_once(&g_open_file_limit_once, raise_open_file_limit);

    swipterm_runtime *rt = calloc(1, sizeof(*rt));
    if (!rt) {
        return -LINUX_ENOMEM;
    }
    rt->callbacks = options->callbacks;
    rt->console_log = getenv("SWIPTERM_CONSOLE_LOG") != NULL;
    rt->network = options->network;
    swipterm_network_retain(rt->network);
    rt->root_lock_fd = -1;
    rt->control_fd = -1;
    rt->shutdown_pipe[0] = -1;
    rt->shutdown_pipe[1] = -1;
    pthread_mutex_init(&rt->control_tx_lock, NULL);
    pthread_mutex_init(&rt->state_lock, NULL);
    pthread_cond_init(&rt->hello_cond, NULL);
    pthread_cond_init(&rt->halt_cond, NULL);
    pthread_cond_init(&rt->ready_cond, NULL);
    pthread_cond_init(&rt->console_cond, NULL);
    pthread_cond_init(&rt->workers_cond, NULL);
    if (pipe(rt->shutdown_pipe) != 0) {
        swipterm_shutdown(rt);
        return -LINUX_ENOMEM;
    }
    set_cloexec(rt->shutdown_pipe[0]);
    set_cloexec(rt->shutdown_pipe[1]);

    rt->root_lock_fd = open(options->root_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (rt->root_lock_fd < 0 || flock(rt->root_lock_fd, LOCK_EX | LOCK_NB) < 0) {
        int rc = errno == EWOULDBLOCK ? -LINUX_EBUSY : errno == ENOENT ? -LINUX_ENOENT : -LINUX_EIO;
        swipterm_shutdown(rt);
        return rc;
    }

    char disk_path[1024];
    if (snprintf(disk_path, sizeof(disk_path), "%s/root.ext4", options->root_path) >= (int)sizeof(disk_path)) {
        swipterm_shutdown(rt);
        return -LINUX_EINVAL;
    }
    int disk_fd = open(disk_path, O_RDONLY | O_CLOEXEC);
    if (disk_fd < 0) {
        int rc = errno == ENOENT ? -LINUX_ENOENT : -LINUX_EIO;
        swipterm_shutdown(rt);
        return rc;
    }
    boot_plan_t *plan = NULL;
    int rc = boot_plan_resolve(disk_fd, options->kernel, options->kernel_release, &plan, error_message);
    if (rc < 0) {
        close(disk_fd);
        swipterm_shutdown(rt);
        return host_error(rc);
    }

    char cmdline[2048];
    bool has_extra = options->extra_cmdline && options->extra_cmdline[0] != '\0';
    if (snprintf(cmdline, sizeof(cmdline), "console=ttyAMA0 earlycon quiet panic=-1%s%s", has_extra ? " " : "",
                 has_extra ? options->extra_cmdline : "") >= (int)sizeof(cmdline)) {
        boot_plan_free(plan);
        close(disk_fd);
        swipterm_shutdown(rt);
        return -LINUX_EINVAL;
    }

    rc = boot_machine(rt, options, plan, disk_path, cmdline);
    if (rc == 0) {
        snprintf(rt->boot_info.release, sizeof(rt->boot_info.release), "%s", boot_plan_release(plan));
        snprintf(rt->boot_info.fallback_reason, sizeof(rt->boot_info.fallback_reason), "%s", boot_plan_reasons(plan));
    } else if (rc == -LINUX_ENOEXEC && error_message) {
        *error_message = strdup(boot_plan_reasons(plan));
    }
    boot_plan_free(plan);
    close(disk_fd);
    if (rc < 0) {
        swipterm_shutdown(rt);
        return rc;
    }

    size_t cfg_size = 4 + rpc_str_wire_size(options->hostname) +
                      rpc_str_wire_size(options->host_commands_directory);
    if (cfg_size > SWIPTERMD_MAX_BODY_LEN) {
        swipterm_shutdown(rt);
        return -LINUX_EINVAL;
    }
    uint8_t *cfg_buf = malloc(cfg_size);
    if (!cfg_buf) {
        swipterm_shutdown(rt);
        return -LINUX_ENOMEM;
    }
    swiptermd_buf_w_t cfg_w = swiptermd_buf_w_init(cfg_buf, cfg_size);
    swiptermd_enc_configure_init(&cfg_w, SWIPTERMD_INIT_MANAGED,
                                 options->hostname, options->host_commands_directory);
    int cfg_rc = control_rpc_status(rt, SWIPTERMD_OP_CONFIGURE_INIT, cfg_buf, (uint32_t)cfg_w.len);
    free(cfg_buf);
    if (cfg_rc < 0) {
        swipterm_shutdown(rt);
        return cfg_rc;
    }

    *runtime_out = rt;
    return 0;
}

void swipterm_begin_shutdown(swipterm_runtime *rt) {
    if (!rt) {
        return;
    }
    if (atomic_exchange(&rt->halt_initiated, true)) {
        return;
    }
    atomic_store(&rt->shutting_down, true);

    if (rt->vm != NULL && rt->control_fd >= 0 && !atomic_load(&rt->vm_powered_off)) {
        (void)vmm_resume(rt->vm);
        uint8_t halt_req[SWIPTERMD_HALT_REQ_SIZE];
        swiptermd_enc_halt_req(halt_req, 0);
        if (control_send(rt, SWIPTERMD_OP_HALT, 0, halt_req, sizeof(halt_req))) {
            struct timespec deadline;
            clock_gettime(CLOCK_REALTIME, &deadline);
            deadline.tv_sec += SHUTDOWN_TIMEOUT_SEC;
            pthread_mutex_lock(&rt->state_lock);
            while (!atomic_load(&rt->vm_powered_off)) {
                if (pthread_cond_timedwait(&rt->halt_cond, &rt->state_lock, &deadline) == ETIMEDOUT) {
                    break;
                }
            }
            clock_gettime(CLOCK_REALTIME, &deadline);
            deadline.tv_sec += STOP_REPORT_TIMEOUT_SEC;
            while (!rt->halt_info.shutdown_complete && !rt->control_rx_done) {
                if (pthread_cond_timedwait(&rt->ready_cond, &rt->state_lock, &deadline) == ETIMEDOUT) {
                    break;
                }
            }
            pthread_mutex_unlock(&rt->state_lock);
        }
    }

    pthread_mutex_lock(&rt->state_lock);
    pthread_cond_broadcast(&rt->ready_cond);
    if (rt->shutdown_pipe[1] >= 0) {
        close(rt->shutdown_pipe[1]);
        rt->shutdown_pipe[1] = -1;
    }
    rt->console_active = false;
    pthread_cond_broadcast(&rt->console_cond);
    for (swipterm_terminal *t = rt->terminals; t != NULL; t = t->next) {
        pthread_mutex_lock(&t->lock);
        t->closed = true;
        t->eof = true;
        int fd = t->data_fd;
        pthread_cond_broadcast(&t->ring_cond);
        pthread_cond_broadcast(&t->attach_cond);
        pthread_mutex_unlock(&t->lock);
        if (fd >= 0) {
            (void)shutdown(fd, SHUT_RDWR);
        }
        terminal_notify(t);
    }
    pthread_mutex_unlock(&rt->state_lock);

    fail_all_pending_reqs(rt);

    if (rt->vsock) {
        virtio_vsock_unlisten_host(rt->vsock, SWIPTERMD_DATA_PORT);
        virtio_vsock_notify_halt(rt->vsock);
    }

    pthread_mutex_lock(&rt->control_tx_lock);
    if (rt->control_fd >= 0) {
        (void)shutdown(rt->control_fd, SHUT_RDWR);
    }
    pthread_mutex_unlock(&rt->control_tx_lock);
}

void swipterm_shutdown(swipterm_runtime *rt) {
    if (!rt) {
        return;
    }
    if (atomic_exchange(&rt->finalized, true)) {
        return;
    }

    swipterm_begin_shutdown(rt);

    if (rt->control_rx_started) {
        pthread_join(rt->control_rx_thread, NULL);
        rt->control_rx_started = false;
    }

    if (rt->vsock) {
        virtio_vsock_notify_halt(rt->vsock);
    }
    swipterm_network_set_runtime(rt->network, NULL);

    if (rt->vm) {
        vmm_stop(rt->vm);
        vmm_destroy(rt->vm);
        rt->vm = NULL;
    }

    pthread_mutex_lock(&rt->control_tx_lock);
    if (rt->control_fd >= 0) {
        close(rt->control_fd);
        rt->control_fd = -1;
    }
    pthread_mutex_unlock(&rt->control_tx_lock);

    if (rt->root_lock_fd >= 0) {
        close(rt->root_lock_fd);
        rt->root_lock_fd = -1;
    }

    pthread_mutex_lock(&rt->state_lock);
    for (swipterm_terminal *t = rt->terminals; t != NULL; t = t->next) {
        pthread_mutex_lock(&t->lock);
        t->closed = true;
        t->eof = true;
        int fd = t->data_fd;
        pthread_cond_broadcast(&t->ring_cond);
        pthread_cond_broadcast(&t->attach_cond);
        pthread_mutex_unlock(&t->lock);
        if (fd >= 0) {
            (void)shutdown(fd, SHUT_RDWR);
        }
        terminal_notify(t);
    }
    for (swipterm_invocation *inv = rt->invocations; inv != NULL; inv = inv->next) {
        inv->cancelled = true;
    }
    while (rt->active_workers > 0) {
        pthread_cond_wait(&rt->workers_cond, &rt->state_lock);
    }

    stream_route_t *routes = rt->routes;
    rt->routes = NULL;
    swipterm_process *processes = rt->processes;
    rt->processes = NULL;
    swipterm_terminal *terminals = rt->terminals;
    rt->terminals = NULL;
    mount_entry_t *mounts = rt->mounts;
    rt->mounts = NULL;
    command_entry_t *commands = rt->commands;
    rt->commands = NULL;
    swipterm_invocation *invocations = rt->invocations;
    rt->invocations = NULL;
    device_entry_t *devices = rt->devices;
    rt->devices = NULL;
    device_file_entry_t *device_files = rt->device_files;
    rt->device_files = NULL;
    pthread_mutex_unlock(&rt->state_lock);

    for (swipterm_process *p = processes; p != NULL; p = p->next) {
        pthread_mutex_lock(&p->rt_lock);
        p->rt = NULL;
        pthread_mutex_unlock(&p->rt_lock);
    }
    for (swipterm_terminal *t = terminals; t != NULL; t = t->next) {
        pthread_mutex_lock(&t->rt_lock);
        t->rt = NULL;
        pthread_mutex_unlock(&t->rt_lock);
    }
    for (swipterm_invocation *inv = invocations; inv != NULL; inv = inv->next) {
        pthread_mutex_lock(&inv->rt_lock);
        inv->rt = NULL;
        pthread_mutex_unlock(&inv->rt_lock);
        if (inv->swift_context && rt->callbacks.command_abort) {
            rt->callbacks.command_abort(inv->swift_context);
        }
    }

    while (routes) {
        stream_route_t *r = routes;
        routes = r->next;
        if (r->peer_fd >= 0) close(r->peer_fd);
        if (r->vsock_fd >= 0) close(r->vsock_fd);
        if (r->inv) signal_invocation_flush(r->inv);
        free(r);
    }
    while (processes) {
        swipterm_process *p = processes;
        processes = p->next;
        bool should_notify = p->spawned && !atomic_exchange(&p->exited, true);
        if (p->terminal) {
            pthread_mutex_lock(&p->terminal->lock);
            p->terminal->in_use = false;
            pthread_mutex_unlock(&p->terminal->lock);
            terminal_release_internal(p->terminal);
            p->terminal = NULL;
        }
        if (should_notify && rt->callbacks.process_exit) {
            rt->callbacks.process_exit(p->user_context, 9);
        }
        process_release_internal(p);
    }
    while (terminals) {
        swipterm_terminal *t = terminals;
        terminals = t->next;
        terminal_release_internal(t);
    }
    while (mounts) {
        mount_entry_t *m = mounts;
        mounts = m->next;
        free(m->guest_path);
        free(m);
    }
    while (commands) {
        command_entry_t *c = commands;
        commands = c->next;
        free(c->name);
        free(c);
    }
    while (invocations) {
        swipterm_invocation *inv = invocations;
        invocations = inv->next;
        invocation_release_internal(inv);
    }
    while (devices) {
        device_entry_t *d = devices;
        devices = d->next;
        free(d);
    }
    while (device_files) {
        device_file_entry_t *fe = device_files;
        device_files = fe->next;
        device_file_release(fe);
    }

    if (rt->shutdown_pipe[0] >= 0) {
        close(rt->shutdown_pipe[0]);
    }
    pthread_mutex_destroy(&rt->control_tx_lock);
    pthread_mutex_destroy(&rt->state_lock);
    pthread_cond_destroy(&rt->hello_cond);
    pthread_cond_destroy(&rt->halt_cond);
    pthread_cond_destroy(&rt->ready_cond);
    pthread_cond_destroy(&rt->console_cond);
    pthread_cond_destroy(&rt->workers_cond);
    swipterm_network_release(rt->network);
    free(rt);
}

int swipterm_start_init(swipterm_runtime *rt, const char *path,
                        size_t argc, const char *const *argv, const char *const *envp) {
    if (!rt || argc > UINT32_MAX) {
        return -LINUX_EINVAL;
    }
    uint32_t mode = path ? SWIPTERMD_INIT_PROCESS : SWIPTERMD_INIT_MANAGED;
    uint8_t has_console = (mode == SWIPTERMD_INIT_PROCESS && rt->callbacks.console_output != NULL) ? 1 : 0;
    uint64_t console_token = has_console ? (atomic_fetch_add(&rt->next_token, 1) + 1) : 0;
    uint32_t argc32 = argv ? (uint32_t)argc : 0;
    uint32_t envc = rpc_cstr_list_count(envp);

    size_t req_size = 4 + 8 + 1 + rpc_str_wire_size(path) +
                      rpc_strv_wire_size(argv, argc32) +
                      rpc_strv_wire_size(envp, envc);
    if (req_size > SWIPTERMD_MAX_BODY_LEN) {
        return -LINUX_EINVAL;
    }
    uint8_t *req_buf = malloc(req_size);
    if (!req_buf) {
        return -LINUX_ENOMEM;
    }
    swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
    swiptermd_enc_start_init(&w, mode, console_token, has_console, path, argv, argc32, envp, envc);

    if (has_console) {
        pthread_mutex_lock(&rt->state_lock);
        rt->console_active = true;
        pthread_mutex_unlock(&rt->state_lock);
    }

    int rc = control_rpc_status(rt, SWIPTERMD_OP_START_INIT, req_buf, (uint32_t)w.len);
    free(req_buf);
    if (rc < 0 && has_console) {
        pthread_mutex_lock(&rt->state_lock);
        rt->console_active = false;
        pthread_cond_broadcast(&rt->console_cond);
        pthread_mutex_unlock(&rt->state_lock);
    }
    return rc;
}

void swipterm_suspend(swipterm_runtime *rt) {
    if (!rt || !rt->vm || atomic_load_explicit(&rt->vm->suspended, memory_order_acquire)) {
        return;
    }
    pthread_mutex_lock(&rt->state_lock);
    bool guest_ready = rt->hello_done && !atomic_load(&rt->halted);
    pthread_mutex_unlock(&rt->state_lock);
    if (guest_ready) {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += SUSPEND_SYNC_TIMEOUT_SEC;
        (void)control_rpc(rt, SWIPTERMD_OP_SUSPEND, NULL, 0, NULL, NULL, &deadline);
    }
    (void)vmm_suspend(rt->vm);
}

void swipterm_resume(swipterm_runtime *rt) {
    if (!rt || !rt->vm) {
        return;
    }
    (void)vmm_resume(rt->vm);
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    uint8_t req[SWIPTERMD_RESUME_REQ_SIZE];
    swiptermd_enc_resume_req(req, (int64_t)now.tv_sec * 1000000000LL + now.tv_nsec);
    (void)control_send(rt, SWIPTERMD_OP_RESUME, 0, req, sizeof(req));
}

int swipterm_wait_ready(swipterm_runtime *rt, swipterm_readiness readiness, int timeout_ms) {
    if (!rt || (readiness != SWIPTERM_READY_LOGIN && readiness != SWIPTERM_READY_SYSTEM)) {
        return -LINUX_EINVAL;
    }
    uint32_t bit = 1U << (readiness == SWIPTERM_READY_LOGIN ? SWIPTERMD_READY_LOGIN : SWIPTERMD_READY_SYSTEM);
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    int rc = 0;
    pthread_mutex_lock(&rt->state_lock);
    if (!(rt->guest_caps & SWIPTERMD_CAP_READY)) {
        rc = -LINUX_ENOSYS;
    }
    while (rc == 0 && !(rt->ready_mask & bit)) {
        if (atomic_load(&rt->shutting_down)) {
            rc = -LINUX_ECANCELED;
        } else if (atomic_load(&rt->halted) || rt->control_rx_done || rt->halt_info.reason != SWIPTERM_HALT_NONE) {
            rc = -LINUX_ESRCH;
        } else if (timeout_ms == 0) {
            rc = -LINUX_EAGAIN;
        } else if (timeout_ms < 0) {
            pthread_cond_wait(&rt->ready_cond, &rt->state_lock);
        } else if (pthread_cond_timedwait(&rt->ready_cond, &rt->state_lock, &deadline) == ETIMEDOUT) {
            rc = (rt->ready_mask & bit) ? 0 : -LINUX_ETIMEDOUT;
        }
    }
    pthread_mutex_unlock(&rt->state_lock);
    return rc;
}

void swipterm_get_boot_info(swipterm_runtime *rt, swipterm_boot_info *info_out) {
    if (!rt || !info_out) {
        return;
    }
    *info_out = rt->boot_info;
}

void swipterm_get_halt_info(swipterm_runtime *rt, swipterm_halt_info *info_out) {
    if (!rt || !info_out) {
        return;
    }
    pthread_mutex_lock(&rt->state_lock);
    *info_out = rt->halt_info;
    pthread_mutex_unlock(&rt->state_lock);
}

static int terminal_claim(swipterm_terminal *term) {
    pthread_mutex_lock(&term->lock);
    int rc = term->closed ? -LINUX_EIO : term->in_use ? -LINUX_EBUSY : 0;
    if (rc == 0) {
        term->in_use = true;
    }
    pthread_mutex_unlock(&term->lock);
    return rc;
}

static void terminal_unclaim(swipterm_terminal *term) {
    if (term) {
        pthread_mutex_lock(&term->lock);
        term->in_use = false;
        pthread_mutex_unlock(&term->lock);
    }
}

static int launch_process(swipterm_runtime *rt, uint16_t opcode, uint32_t proc_id, swipterm_terminal *term,
                          void *context, const uint8_t *req, uint32_t req_len,
                          swipterm_process **process_out, int *pid_out) {
    swipterm_process *proc = calloc(1, sizeof(*proc));
    if (!proc) {
        terminal_unclaim(term);
        return -LINUX_ENOMEM;
    }
    pthread_mutex_init(&proc->rt_lock, NULL);
    proc->rt = rt;
    proc->proc_id = proc_id;
    proc->user_context = context;
    if (term) {
        atomic_fetch_add(&term->refcount, 1);
        proc->terminal = term;
    }
    atomic_init(&proc->refcount, 2);
    atomic_init(&proc->exited, false);
    proc->spawned = false;

    pthread_mutex_lock(&rt->state_lock);
    proc->next = rt->processes;
    rt->processes = proc;
    pthread_mutex_unlock(&rt->state_lock);

    uint8_t *reply = NULL;
    uint32_t rlen = 0;
    int rc = control_rpc(rt, opcode, req, req_len, &reply, &rlen, NULL);
    int32_t status = -LINUX_EIO;
    int32_t pid = 0;
    if (rc == 0) {
        if (!swiptermd_dec_spawn_reply(reply, rlen, &status, &pid)) {
            status = -LINUX_EIO;
        }
        free(reply);
    } else {
        status = atomic_load(&rt->halted) ? -LINUX_ESRCH : rc;
    }

    bool removed = false;
    swipterm_terminal *cleanup_term = NULL;
    pthread_mutex_lock(&rt->state_lock);
    if (status >= 0) {
        bool still_listed = false;
        for (swipterm_process *cur = rt->processes; cur != NULL; cur = cur->next) {
            if (cur == proc) {
                still_listed = true;
                break;
            }
        }
        if (still_listed) {
            proc->spawned = true;
        } else if (!atomic_load(&proc->exited)) {
            status = -LINUX_EIO;
        }
    }
    if (status < 0) {
        for (swipterm_process **pp = &rt->processes; *pp; pp = &(*pp)->next) {
            if (*pp == proc) {
                *pp = proc->next;
                removed = true;
                break;
            }
        }
        cleanup_term = proc->terminal;
        proc->terminal = NULL;
    }
    pthread_mutex_unlock(&rt->state_lock);

    if (status < 0) {
        if (removed) {
            pthread_mutex_lock(&proc->rt_lock);
            proc->rt = NULL;
            pthread_mutex_unlock(&proc->rt_lock);
        }
        if (cleanup_term) {
            terminal_unclaim(cleanup_term);
            terminal_release_internal(cleanup_term);
        }
        if (removed) {
            process_release_internal(proc);
        }
        process_release_internal(proc);
        return status;
    }

    proc->pid = pid;
    *pid_out = pid;
    *process_out = proc;
    return 0;
}

int swipterm_spawn(swipterm_runtime *rt, const swipterm_spawn_options *options,
                   swipterm_process **process_out, int *pid_out, int host_fds_out[3]) {
    if (!rt || !options || !options->path || !process_out || !pid_out || !host_fds_out ||
        options->argc > UINT32_MAX) {
        return -LINUX_EINVAL;
    }
    if (atomic_load(&rt->halted)) {
        return -LINUX_ESRCH;
    }

    host_fds_out[0] = -1;
    host_fds_out[1] = -1;
    host_fds_out[2] = -1;

    uint32_t argc32 = options->argv ? (uint32_t)options->argc : 0;
    uint32_t envc = rpc_cstr_list_count(options->envp);
    size_t req_size = 4 + 4 + 12 + 24 +
                      rpc_str_wire_size(options->path) +
                      rpc_str_wire_size(options->working_directory) +
                      rpc_strv_wire_size(options->argv, argc32) +
                      rpc_strv_wire_size(options->envp, envc);
    if (req_size > SWIPTERMD_MAX_BODY_LEN) {
        return -LINUX_EINVAL;
    }

    swipterm_terminal *used_term = NULL;
    for (int i = 0; i < 3; i++) {
        if (options->stdio[i].kind == SWIPTERM_STDIO_TERMINAL) {
            swipterm_terminal *t = options->stdio[i].terminal;
            if (!t || (used_term && used_term != t)) {
                return -LINUX_EINVAL;
            }
            used_term = t;
        }
    }
    if (used_term) {
        int claim = terminal_claim(used_term);
        if (claim < 0) {
            return claim;
        }
    }

    static const uint32_t stdio_roles[3] = {
        SWIPTERMD_STREAM_STDIO_IN,
        SWIPTERMD_STREAM_STDIO_OUT,
        SWIPTERMD_STREAM_STDIO_ERR,
    };

    uint64_t tokens[3] = {0, 0, 0};
    int local_fds[3] = {-1, -1, -1};
    uint8_t *req_buf = malloc(req_size);
    int rc = req_buf ? 0 : -LINUX_ENOMEM;
    for (int i = 0; i < 3 && rc == 0; i++) {
        if (options->stdio[i].kind != SWIPTERM_STDIO_PIPE) {
            continue;
        }
        int sp[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) != 0) {
            rc = -LINUX_ENOMEM;
            break;
        }
        int capacity = 256 * 1024;
        (void)setsockopt(sp[i == 0 ? 0 : 1], SOL_SOCKET, SO_SNDBUF, &capacity, sizeof(capacity));
        set_nosigpipe(sp[0]);
        set_nosigpipe(sp[1]);
        set_cloexec(sp[0]);
        set_cloexec(sp[1]);
        uint64_t token = atomic_fetch_add(&rt->next_token, 1) + 1;
        stream_dir_t dir = (i == 0) ? STREAM_DIR_HOST_TO_GUEST : STREAM_DIR_GUEST_TO_HOST;
        if (!register_stream_route(rt, stdio_roles[i], token, sp[1], dir, NULL)) {
            close(sp[0]);
            close(sp[1]);
            rc = -LINUX_ENOMEM;
            break;
        }
        tokens[i] = token;
        local_fds[i] = sp[0];
    }

    if (rc == 0) {
        uint8_t stdio_mode[3] = {SWIPTERMD_STDIO_NULL, SWIPTERMD_STDIO_NULL, SWIPTERMD_STDIO_NULL};
        uint32_t stdio_pty_id[3] = {0, 0, 0};
        for (int i = 0; i < 3; i++) {
            if (options->stdio[i].kind == SWIPTERM_STDIO_PIPE) {
                stdio_mode[i] = SWIPTERMD_STDIO_PIPE;
            } else if (options->stdio[i].kind == SWIPTERM_STDIO_TERMINAL) {
                stdio_mode[i] = SWIPTERMD_STDIO_PTY;
                stdio_pty_id[i] = options->stdio[i].terminal->pty_id;
            }
        }
        uint32_t proc_id = atomic_fetch_add(&rt->next_proc_id, 1) + 1;
        swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
        swiptermd_enc_spawn_req(&w, proc_id, stdio_mode, stdio_pty_id, tokens,
                                options->path, options->working_directory,
                                options->argv, argc32, options->envp, envc);
        rc = launch_process(rt, SWIPTERMD_OP_SPAWN, proc_id, used_term, options->context,
                            req_buf, (uint32_t)w.len, process_out, pid_out);
    } else {
        terminal_unclaim(used_term);
    }
    free(req_buf);

    if (rc < 0) {
        for (int i = 0; i < 3; i++) {
            if (local_fds[i] >= 0) close(local_fds[i]);
            if (tokens[i] != 0) cancel_stream_route(rt, stdio_roles[i], tokens[i]);
        }
        return rc;
    }
    for (int i = 0; i < 3; i++) {
        host_fds_out[i] = local_fds[i];
    }
    return 0;
}

int swipterm_login(swipterm_runtime *rt, const swipterm_login_options *options,
                   swipterm_process **process_out, int *pid_out) {
    if (!rt || !options || !options->terminal || !process_out || !pid_out) {
        return -LINUX_EINVAL;
    }
    if (atomic_load(&rt->halted)) {
        return -LINUX_ESRCH;
    }
    pthread_mutex_lock(&rt->state_lock);
    bool supported = (rt->guest_caps & SWIPTERMD_CAP_LOGIN) != 0;
    pthread_mutex_unlock(&rt->state_lock);
    if (!supported) {
        return -LINUX_ENOSYS;
    }

    const char *user = options->user ? options->user : "root";
    uint32_t envc = rpc_cstr_list_count(options->envp);
    size_t req_size = 4 + 4 + rpc_str_wire_size(user) + rpc_strv_wire_size(options->envp, envc);
    if (req_size > SWIPTERMD_MAX_BODY_LEN) {
        return -LINUX_EINVAL;
    }
    uint8_t *req_buf = malloc(req_size);
    if (!req_buf) {
        return -LINUX_ENOMEM;
    }
    int rc = terminal_claim(options->terminal);
    if (rc == 0) {
        uint32_t proc_id = atomic_fetch_add(&rt->next_proc_id, 1) + 1;
        swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
        swiptermd_enc_login_req(&w, proc_id, options->terminal->pty_id, user, options->envp, envc);
        rc = launch_process(rt, SWIPTERMD_OP_LOGIN, proc_id, options->terminal, options->context,
                            req_buf, (uint32_t)w.len, process_out, pid_out);
    }
    free(req_buf);
    return rc;
}

int swipterm_process_signal(swipterm_process *proc, int signal, bool group) {
    if (!proc) {
        return -LINUX_EINVAL;
    }
    if (atomic_load(&proc->exited)) {
        return -LINUX_ESRCH;
    }
    swipterm_runtime *rt = enter_child_runtime(&proc->rt_lock, &proc->rt);
    if (!rt) {
        return -LINUX_ESRCH;
    }
    uint8_t req[SWIPTERMD_PROCESS_SIGNAL_REQ_SIZE];
    swiptermd_enc_process_signal(req, proc->proc_id, signal, group);
    int rc = control_rpc_status(rt, SWIPTERMD_OP_PROCESS_SIGNAL, req, sizeof(req));
    worker_leave(rt);
    return rc;
}

void swipterm_process_release(swipterm_process *proc) {
    if (proc) {
        process_release_internal(proc);
    }
}

swipterm_terminal *swipterm_terminal_create(swipterm_runtime *rt,
                                            unsigned short columns, unsigned short rows, int *error_out) {
    if (!rt) {
        if (error_out) *error_out = -LINUX_EINVAL;
        return NULL;
    }
    swipterm_terminal *term = calloc(1, sizeof(*term));
    if (!term) {
        if (error_out) *error_out = -LINUX_ENOMEM;
        return NULL;
    }
    pthread_mutex_init(&term->rt_lock, NULL);
    term->rt = rt;
    term->pty_id = atomic_fetch_add(&rt->next_pty_id, 1) + 1;
    term->token = atomic_fetch_add(&rt->next_token, 1) + 1;
    term->data_fd = -1;
    atomic_init(&term->refcount, 2);
    pthread_mutex_init(&term->lock, NULL);
    pthread_cond_init(&term->ring_cond, NULL);
    pthread_cond_init(&term->attach_cond, NULL);
    pthread_mutex_init(&term->write_lock, NULL);

    if (pipe(term->notify_pipe) != 0) {
        pthread_mutex_destroy(&term->rt_lock);
        pthread_mutex_destroy(&term->lock);
        pthread_cond_destroy(&term->ring_cond);
        pthread_cond_destroy(&term->attach_cond);
        pthread_mutex_destroy(&term->write_lock);
        free(term);
        if (error_out) *error_out = -LINUX_ENOMEM;
        return NULL;
    }
    for (int i = 0; i < 2; i++) {
        int fl = fcntl(term->notify_pipe[i], F_GETFL, 0);
        if (fl >= 0) {
            (void)fcntl(term->notify_pipe[i], F_SETFL, fl | O_NONBLOCK);
        }
        set_cloexec(term->notify_pipe[i]);
    }

    pthread_mutex_lock(&rt->state_lock);
    term->next = rt->terminals;
    rt->terminals = term;
    pthread_mutex_unlock(&rt->state_lock);

    uint8_t req[SWIPTERMD_PTY_CREATE_REQ_SIZE];
    swiptermd_enc_pty_create_req(req, term->pty_id, columns, rows, term->token);

    uint8_t *reply = NULL;
    uint32_t rlen = 0;
    int rc = control_rpc(rt, SWIPTERMD_OP_PTY_CREATE, req, sizeof(req), &reply, &rlen, NULL);

    int32_t status = -LINUX_EIO;
    uint32_t pts_num = 0;
    if (rc == 0) {
        if (!swiptermd_dec_pty_create_reply(reply, rlen, &status, &pts_num)) {
            status = -LINUX_EIO;
        }
        free(reply);
    } else {
        status = rc;
    }
    (void)pts_num;

    if (status < 0) {
        pthread_mutex_lock(&rt->state_lock);
        for (swipterm_terminal **pp = &rt->terminals; *pp; pp = &(*pp)->next) {
            if (*pp == term) {
                *pp = term->next;
                break;
            }
        }
        pthread_mutex_unlock(&rt->state_lock);
        terminal_release_internal(term);
        terminal_release_internal(term);
        if (error_out) *error_out = status;
        return NULL;
    }

    if (error_out) *error_out = 0;
    return term;
}

int swipterm_terminal_notify_fd(swipterm_terminal *term) {
    return term ? term->notify_pipe[0] : -1;
}

ssize_t swipterm_terminal_read(swipterm_terminal *term, void *buffer, size_t length) {
    if (!term || !buffer || length == 0) {
        return -LINUX_EINVAL;
    }
    pthread_mutex_lock(&term->lock);
    if (term->count == 0) {
        bool done = term->eof || term->closed;
        pthread_mutex_unlock(&term->lock);
        return done ? 0 : -LINUX_EAGAIN;
    }
    size_t to_read = length < term->count ? length : term->count;
    size_t first = TERMINAL_RING_CAPACITY - term->head;
    if (first > to_read) {
        first = to_read;
    }
    memcpy(buffer, term->ring + term->head, first);
    if (to_read > first) {
        memcpy((uint8_t *)buffer + first, term->ring, to_read - first);
    }
    term->head = (term->head + to_read) % TERMINAL_RING_CAPACITY;
    bool was_full = term->count == TERMINAL_RING_CAPACITY;
    term->count -= to_read;
    if (was_full) {
        pthread_cond_signal(&term->ring_cond);
    }
    pthread_mutex_unlock(&term->lock);
    return (ssize_t)to_read;
}

int swipterm_terminal_write(swipterm_terminal *term, const void *data, size_t length) {
    if (!term || (!data && length > 0)) {
        return -LINUX_EINVAL;
    }
    if (length == 0) {
        return 0;
    }

    pthread_mutex_lock(&term->lock);
    while (!term->attached && !term->closed && !term->eof) {
        pthread_cond_wait(&term->attach_cond, &term->lock);
    }
    int fd = (!term->closed && !term->eof) ? term->data_fd : -1;
    pthread_mutex_unlock(&term->lock);

    if (fd < 0) {
        return -LINUX_EIO;
    }
    pthread_mutex_lock(&term->write_lock);
    bool ok = rpc_write_all(fd, data, length);
    pthread_mutex_unlock(&term->write_lock);
    return ok ? 0 : -LINUX_EIO;
}

void swipterm_terminal_resize(swipterm_terminal *term, unsigned short columns, unsigned short rows) {
    if (!term) {
        return;
    }
    swipterm_runtime *rt = enter_child_runtime(&term->rt_lock, &term->rt);
    if (!rt) {
        return;
    }
    uint8_t req[SWIPTERMD_PTY_RESIZE_REQ_SIZE];
    swiptermd_enc_pty_resize(req, term->pty_id, columns, rows);
    (void)control_rpc_status(rt, SWIPTERMD_OP_PTY_RESIZE, req, sizeof(req));
    worker_leave(rt);
}

void swipterm_terminal_close(swipterm_terminal *term) {
    if (!term) {
        return;
    }
    pthread_mutex_lock(&term->lock);
    bool already_closed = term->closed;
    term->closed = true;
    int fd = term->data_fd;
    pthread_cond_broadcast(&term->ring_cond);
    pthread_cond_broadcast(&term->attach_cond);
    pthread_mutex_unlock(&term->lock);

    if (fd >= 0) {
        (void)shutdown(fd, SHUT_RDWR);
    }
    terminal_notify(term);

    if (!already_closed) {
        swipterm_runtime *rt = enter_child_runtime(&term->rt_lock, &term->rt);
        if (rt) {
            uint8_t req[SWIPTERMD_PTY_CLOSE_REQ_SIZE];
            swiptermd_enc_pty_close(req, term->pty_id);
            (void)control_rpc_status(rt, SWIPTERMD_OP_PTY_CLOSE, req, sizeof(req));
            worker_leave(rt);
        }
    }
}

void swipterm_terminal_release(swipterm_terminal *term) {
    if (!term) {
        return;
    }
    swipterm_runtime *rt = enter_child_runtime(&term->rt_lock, &term->rt);
    if (rt) {
        bool removed = false;
        pthread_mutex_lock(&rt->state_lock);
        for (swipterm_terminal **pp = &rt->terminals; *pp; pp = &(*pp)->next) {
            if (*pp == term) {
                *pp = term->next;
                removed = true;
                break;
            }
        }
        pthread_mutex_unlock(&rt->state_lock);
        if (removed) {
            pthread_mutex_lock(&term->rt_lock);
            term->rt = NULL;
            pthread_mutex_unlock(&term->rt_lock);
            terminal_release_internal(term);
        }
        worker_leave(rt);
    }
    terminal_release_internal(term);
}

int swipterm_stat_path(swipterm_runtime *rt, const char *path,
                       bool follow_symlinks, swipterm_stat *stat_out) {
    if (!rt || !path || !stat_out) {
        return -LINUX_EINVAL;
    }
    size_t req_size = 1 + rpc_str_wire_size(path);
    if (req_size > SWIPTERMD_MAX_BODY_LEN) {
        return -LINUX_EINVAL;
    }
    uint8_t *req_buf = malloc(req_size);
    if (!req_buf) {
        return -LINUX_ENOMEM;
    }
    swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
    swiptermd_enc_fs_stat_req(&w, follow_symlinks, path);

    uint8_t *reply = NULL;
    uint32_t rlen = 0;
    int rc = control_rpc(rt, SWIPTERMD_OP_FS_STAT, req_buf, (uint32_t)w.len, &reply, &rlen, NULL);
    free(req_buf);
    if (rc < 0) {
        return rc;
    }

    swiptermd_buf_r_t r = swiptermd_buf_r_init(reply, rlen);
    int32_t status = swiptermd_r_i32(&r);
    if (r.underflow) {
        status = -LINUX_EIO;
    } else if (status == 0) {
        swiptermd_stat_wire_t st;
        swiptermd_stat_decode(&r, &st);
        if (r.underflow) {
            status = -LINUX_EIO;
        } else {
            *stat_out = (swipterm_stat){
                .mode = st.mode,
                .uid = st.uid,
                .gid = st.gid,
                .nlink = st.nlink,
                .size = st.size,
                .inode = st.inode,
                .rdev = st.rdev,
                .access_time = {.tv_sec = (time_t)st.atime_sec, .tv_nsec = (long)st.atime_nsec},
                .modification_time = {.tv_sec = (time_t)st.mtime_sec, .tv_nsec = (long)st.mtime_nsec},
                .change_time = {.tv_sec = (time_t)st.ctime_sec, .tv_nsec = (long)st.ctime_nsec},
            };
        }
    }
    free(reply);
    return status;
}

int swipterm_read_file(swipterm_runtime *rt, const char *path, void **data_out, size_t *length_out) {
    if (!rt || !path || !data_out || !length_out) {
        return -LINUX_EINVAL;
    }
    *data_out = NULL;
    *length_out = 0;

    size_t req_size = rpc_str_wire_size(path) + 8 + 4;
    if (req_size > SWIPTERMD_MAX_BODY_LEN) {
        return -LINUX_EINVAL;
    }
    uint8_t *req_buf = malloc(req_size);
    if (!req_buf) {
        return -LINUX_ENOMEM;
    }

    uint8_t *acc = NULL;
    size_t acc_len = 0;
    size_t acc_cap = 0;
    uint64_t offset = 0;
    int32_t status = 0;

    for (;;) {
        swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
        swiptermd_enc_fs_read_req(&w, path, offset, FS_READ_CHUNK_SIZE);

        uint8_t *reply = NULL;
        uint32_t rlen = 0;
        int rc = control_rpc(rt, SWIPTERMD_OP_FS_READ, req_buf, (uint32_t)w.len, &reply, &rlen, NULL);
        if (rc < 0) {
            status = rc;
            break;
        }

        bool eof = false;
        swiptermd_slice_t chunk = {NULL, 0};
        if (!swiptermd_dec_fs_read_reply(reply, rlen, &status, &eof, &chunk)) {
            status = -LINUX_EIO;
            free(reply);
            break;
        }
        if (status < 0) {
            free(reply);
            break;
        }
        if (chunk.len > 0) {
            if (acc_len + chunk.len > acc_cap) {
                size_t ncap = acc_cap ? acc_cap * 2 : (chunk.len > 4096 ? chunk.len : 4096);
                while (ncap < acc_len + chunk.len) {
                    ncap *= 2;
                }
                uint8_t *next = realloc(acc, ncap);
                if (!next) {
                    status = -LINUX_ENOMEM;
                    free(reply);
                    break;
                }
                acc = next;
                acc_cap = ncap;
            }
            memcpy(acc + acc_len, chunk.ptr, chunk.len);
            acc_len += chunk.len;
            offset += chunk.len;
        }
        free(reply);
        if (eof || chunk.len == 0) {
            break;
        }
    }

    free(req_buf);
    if (status < 0) {
        free(acc);
        return status;
    }
    if (!acc) {
        acc = malloc(1);
        if (!acc) {
            return -LINUX_ENOMEM;
        }
    }
    *data_out = acc;
    *length_out = acc_len;
    return 0;
}

int swipterm_write_file(swipterm_runtime *rt, const char *path,
                        const void *data, size_t length, uint32_t mode) {
    if (!rt || !path || (length > 0 && !data)) {
        return -LINUX_EINVAL;
    }
    size_t hdr_size = 4 + 4 + rpc_str_wire_size(path) + 4;
    if (hdr_size >= SWIPTERMD_MAX_BODY_LEN) {
        return -LINUX_EINVAL;
    }
    size_t max_chunk = SWIPTERMD_MAX_BODY_LEN - hdr_size;
    if (max_chunk > FS_WRITE_CHUNK_SIZE) {
        max_chunk = FS_WRITE_CHUNK_SIZE;
    }

    const uint8_t *src = data;
    size_t offset = 0;
    do {
        size_t chunk_len = length - offset;
        if (chunk_len > max_chunk) {
            chunk_len = max_chunk;
        }
        uint32_t flags = (offset == 0 ? 0 : SWIPTERMD_FS_WRITE_FLAG_APPEND) |
                         (offset + chunk_len == length ? SWIPTERMD_FS_WRITE_FLAG_SYNC : 0);
        size_t req_size = hdr_size + chunk_len;
        uint8_t *req_buf = malloc(req_size);
        if (!req_buf) {
            return -LINUX_ENOMEM;
        }
        swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
        swiptermd_enc_fs_write_req(&w, mode, flags, path, src ? src + offset : NULL, (uint32_t)chunk_len);
        int rc = control_rpc_status(rt, SWIPTERMD_OP_FS_WRITE, req_buf, (uint32_t)w.len);
        free(req_buf);
        if (rc < 0) {
            return rc;
        }
        offset += chunk_len;
    } while (offset < length);

    return 0;
}

int swipterm_list_directory(swipterm_runtime *rt, const char *path,
                            swipterm_directory_entry **entries_out, size_t *count_out) {
    if (!rt || !path || !entries_out || !count_out) {
        return -LINUX_EINVAL;
    }
    *entries_out = NULL;
    *count_out = 0;

    size_t req_size = rpc_str_wire_size(path);
    if (req_size > SWIPTERMD_MAX_BODY_LEN) {
        return -LINUX_EINVAL;
    }
    uint8_t *req_buf = malloc(req_size);
    if (!req_buf) {
        return -LINUX_ENOMEM;
    }
    swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
    swiptermd_w_str(&w, path);

    uint8_t *reply = NULL;
    uint32_t rlen = 0;
    int rc = control_rpc(rt, SWIPTERMD_OP_FS_LIST, req_buf, (uint32_t)w.len, &reply, &rlen, NULL);
    free(req_buf);
    if (rc < 0) {
        return rc;
    }

    swiptermd_buf_r_t r = swiptermd_buf_r_init(reply, rlen);
    int32_t status = swiptermd_r_i32(&r);
    if (r.underflow) {
        status = -LINUX_EIO;
    } else if (status == 0) {
        uint32_t count = swiptermd_r_u32(&r);
        if (r.underflow) {
            status = -LINUX_EIO;
        } else if (count > 0) {
            swipterm_directory_entry *entries = calloc(count, sizeof(*entries));
            if (!entries) {
                status = -LINUX_ENOMEM;
            } else {
                for (uint32_t i = 0; i < count; i++) {
                    swiptermd_slice_t name = {NULL, 0};
                    if (!swiptermd_fs_list_entry_decode(&r, &entries[i].mode, &name)) {
                        swipterm_free_directory_entries(entries, i + 1);
                        entries = NULL;
                        status = -LINUX_EIO;
                        break;
                    }
                    entries[i].name = malloc((size_t)name.len + 1);
                    if (!entries[i].name) {
                        swipterm_free_directory_entries(entries, i + 1);
                        entries = NULL;
                        status = -LINUX_ENOMEM;
                        break;
                    }
                    if (name.len > 0) {
                        memcpy(entries[i].name, name.ptr, name.len);
                    }
                    entries[i].name[name.len] = '\0';
                }
                if (status == 0) {
                    *entries_out = entries;
                    *count_out = count;
                }
            }
        }
    }
    free(reply);
    return status;
}

void swipterm_free_directory_entries(swipterm_directory_entry *entries, size_t count) {
    if (!entries) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(entries[i].name);
    }
    free(entries);
}

int swipterm_create_directory(swipterm_runtime *rt, const char *path, uint32_t mode, bool intermediates) {
    if (!rt || !path) {
        return -LINUX_EINVAL;
    }
    size_t req_size = 4 + 1 + rpc_str_wire_size(path);
    if (req_size > SWIPTERMD_MAX_BODY_LEN) {
        return -LINUX_EINVAL;
    }
    uint8_t *req_buf = malloc(req_size);
    if (!req_buf) {
        return -LINUX_ENOMEM;
    }
    swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
    swiptermd_enc_fs_mkdir_req(&w, mode, intermediates, path);
    int rc = control_rpc_status(rt, SWIPTERMD_OP_FS_MKDIR, req_buf, (uint32_t)w.len);
    free(req_buf);
    return rc;
}

int swipterm_remove(swipterm_runtime *rt, const char *path, bool recursive) {
    if (!rt || !path) {
        return -LINUX_EINVAL;
    }
    size_t req_size = 1 + rpc_str_wire_size(path);
    if (req_size > SWIPTERMD_MAX_BODY_LEN) {
        return -LINUX_EINVAL;
    }
    uint8_t *req_buf = malloc(req_size);
    if (!req_buf) {
        return -LINUX_ENOMEM;
    }
    swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
    swiptermd_enc_fs_remove_req(&w, recursive, path);
    int rc = control_rpc_status(rt, SWIPTERMD_OP_FS_REMOVE, req_buf, (uint32_t)w.len);
    free(req_buf);
    return rc;
}

int swipterm_mount(swipterm_runtime *rt, const char *host_path, const char *guest_path, bool read_only) {
    if (!rt || !rt->vm || !host_path || !guest_path) {
        return -LINUX_EINVAL;
    }
    if (!guest_has_capability(rt, SWIPTERMD_CAP_VIRTIOFS)) {
        return -LINUX_ENODEV;
    }
    struct stat host_st;
    bool is_directory = (stat(host_path, &host_st) == 0 && S_ISDIR(host_st.st_mode));

    uint32_t mid = atomic_fetch_add(&rt->next_mount_id, 1);
    char tag[32];
    snprintf(tag, sizeof(tag), "swipfs%u", mid);

    size_t req_size = 1 + 1 + rpc_str_wire_size(tag) + rpc_str_wire_size(guest_path);
    if (req_size > SWIPTERMD_MAX_BODY_LEN) {
        return -LINUX_EINVAL;
    }

    mount_entry_t *entry = calloc(1, sizeof(*entry));
    if (!entry) {
        return -LINUX_ENOMEM;
    }
    entry->guest_path = strdup(guest_path);
    if (!entry->guest_path) {
        free(entry);
        return -LINUX_ENOMEM;
    }
    uint8_t *req_buf = malloc(req_size);
    if (!req_buf) {
        free(entry->guest_path);
        free(entry);
        return -LINUX_ENOMEM;
    }

    uint32_t slot = 0;
    int attach_rc = vmm_attach_virtio_fs(rt->vm, tag, host_path, read_only, &slot);
    if (attach_rc < 0) {
        free(req_buf);
        free(entry->guest_path);
        free(entry);
        return attach_rc;
    }

    swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
    swiptermd_enc_fs_mount_virtiofs_req(&w, read_only, is_directory, tag, guest_path);
    int rc = control_rpc_status(rt, SWIPTERMD_OP_FS_MOUNT_VIRTIOFS, req_buf, (uint32_t)w.len);
    free(req_buf);

    if (rc < 0) {
        vmm_detach_virtio_fs(rt->vm, slot);
        free(entry->guest_path);
        free(entry);
        return rc;
    }

    memcpy(entry->tag, tag, sizeof(entry->tag));
    entry->slot = slot;
    pthread_mutex_lock(&rt->state_lock);
    entry->next = rt->mounts;
    rt->mounts = entry;
    pthread_mutex_unlock(&rt->state_lock);
    return 0;
}

int swipterm_unmount(swipterm_runtime *rt, const char *guest_path) {
    if (!rt || !rt->vm || !guest_path) {
        return -LINUX_EINVAL;
    }
    size_t req_size = rpc_str_wire_size(guest_path);
    if (req_size > SWIPTERMD_MAX_BODY_LEN) {
        return -LINUX_EINVAL;
    }
    uint8_t *req_buf = malloc(req_size);
    if (!req_buf) {
        return -LINUX_ENOMEM;
    }
    swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
    swiptermd_w_str(&w, guest_path);
    int rc = control_rpc_status(rt, SWIPTERMD_OP_FS_UNMOUNT, req_buf, (uint32_t)w.len);
    free(req_buf);
    if (rc < 0) {
        return rc;
    }

    uint32_t slot = 0;
    bool found = false;
    pthread_mutex_lock(&rt->state_lock);
    for (mount_entry_t **pp = &rt->mounts; *pp; pp = &(*pp)->next) {
        if (strcmp((*pp)->guest_path, guest_path) == 0) {
            mount_entry_t *m = *pp;
            *pp = m->next;
            slot = m->slot;
            found = true;
            free(m->guest_path);
            free(m);
            break;
        }
    }
    pthread_mutex_unlock(&rt->state_lock);
    if (found) {
        vmm_detach_virtio_fs(rt->vm, slot);
    }
    return 0;
}

int swipterm_register_command(swipterm_runtime *rt, const char *name, void *context) {
    if (!rt || !name || name[0] == '\0') {
        return -LINUX_EINVAL;
    }

    command_entry_t *entry = calloc(1, sizeof(*entry));
    if (!entry) {
        return -LINUX_ENOMEM;
    }
    entry->name = strdup(name);
    entry->user_context = context;
    if (!entry->name) {
        free(entry);
        return -LINUX_ENOMEM;
    }

    pthread_mutex_lock(&rt->state_lock);
    for (command_entry_t *c = rt->commands; c != NULL; c = c->next) {
        if (strcmp(c->name, name) == 0) {
            pthread_mutex_unlock(&rt->state_lock);
            free(entry->name);
            free(entry);
            return -LINUX_EEXIST;
        }
    }
    entry->next = rt->commands;
    rt->commands = entry;
    pthread_mutex_unlock(&rt->state_lock);

    size_t req_size = rpc_str_wire_size(name);
    uint8_t *req_buf = req_size <= SWIPTERMD_MAX_BODY_LEN ? malloc(req_size) : NULL;
    int rc = req_buf ? 0 : (req_size > SWIPTERMD_MAX_BODY_LEN ? -LINUX_EINVAL : -LINUX_ENOMEM);
    if (req_buf) {
        swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
        swiptermd_w_str(&w, name);
        rc = control_rpc_status(rt, SWIPTERMD_OP_REGISTER_CMD, req_buf, (uint32_t)w.len);
        free(req_buf);
    }

    if (rc < 0) {
        pthread_mutex_lock(&rt->state_lock);
        for (command_entry_t **pp = &rt->commands; *pp; pp = &(*pp)->next) {
            if (*pp == entry) {
                *pp = entry->next;
                break;
            }
        }
        registration_wait_unused(rt, &entry->users);
        pthread_mutex_unlock(&rt->state_lock);
        free(entry->name);
        free(entry);
    }
    return rc;
}

void swipterm_invocation_finish(swipterm_invocation *inv, int status) {
    if (!inv) {
        return;
    }
    swipterm_runtime *rt = enter_child_runtime(&inv->rt_lock, &inv->rt);
    if (!rt) {
        return;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 2;
    pthread_mutex_lock(&inv->out_flush.lock);
    while (inv->out_flush.remaining > 0 && !atomic_load(&rt->shutting_down)) {
        if (pthread_cond_timedwait(&inv->out_flush.cond, &inv->out_flush.lock, &ts) == ETIMEDOUT) {
            break;
        }
    }
    pthread_mutex_unlock(&inv->out_flush.lock);

    uint8_t msg[SWIPTERMD_CMD_FINISH_SIZE];
    swiptermd_enc_cmd_finish(msg, inv->inv_id, status);
    (void)control_send(rt, SWIPTERMD_OP_CMD_FINISH, 0, msg, sizeof(msg));

    bool removed = false;
    pthread_mutex_lock(&rt->state_lock);
    for (swipterm_invocation **pp = &rt->invocations; *pp; pp = &(*pp)->next) {
        if (*pp == inv) {
            *pp = inv->next;
            removed = true;
            break;
        }
    }
    pthread_mutex_unlock(&rt->state_lock);
    if (removed) {
        pthread_mutex_lock(&inv->rt_lock);
        inv->rt = NULL;
        pthread_mutex_unlock(&inv->rt_lock);
        invocation_release_internal(inv);
    }
    worker_leave(rt);
}

void swipterm_invocation_release(swipterm_invocation *inv) {
    if (inv) {
        invocation_release_internal(inv);
    }
}

int swipterm_register_device(swipterm_runtime *rt, const char *path, void *context) {
    if (!rt || !path || path[0] == '\0') {
        return -LINUX_EINVAL;
    }
    if (!guest_has_capability(rt, SWIPTERMD_CAP_DEVICES)) {
        return -LINUX_ENODEV;
    }
    uint32_t dev_id = atomic_fetch_add(&rt->next_dev_id, 1) + 1;

    device_entry_t *entry = calloc(1, sizeof(*entry));
    if (!entry) {
        return -LINUX_ENOMEM;
    }
    entry->dev_id = dev_id;
    entry->user_context = context;

    pthread_mutex_lock(&rt->state_lock);
    entry->next = rt->devices;
    rt->devices = entry;
    pthread_mutex_unlock(&rt->state_lock);

    size_t req_size = 4 + rpc_str_wire_size(path);
    uint8_t *req_buf = req_size <= SWIPTERMD_MAX_BODY_LEN ? malloc(req_size) : NULL;
    int rc = req_buf ? 0 : (req_size > SWIPTERMD_MAX_BODY_LEN ? -LINUX_EINVAL : -LINUX_ENOMEM);
    if (req_buf) {
        swiptermd_buf_w_t w = swiptermd_buf_w_init(req_buf, req_size);
        swiptermd_enc_register_device(&w, dev_id, path);
        rc = control_rpc_status(rt, SWIPTERMD_OP_REGISTER_DEVICE, req_buf, (uint32_t)w.len);
        free(req_buf);
    }

    if (rc < 0) {
        pthread_mutex_lock(&rt->state_lock);
        for (device_entry_t **pp = &rt->devices; *pp; pp = &(*pp)->next) {
            if (*pp == entry) {
                *pp = entry->next;
                break;
            }
        }
        registration_wait_unused(rt, &entry->users);
        pthread_mutex_unlock(&rt->state_lock);
        free(entry);
    }
    return rc;
}
