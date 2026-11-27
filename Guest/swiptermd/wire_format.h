#ifndef SWIPTERMD_WIRE_FORMAT_H
#define SWIPTERMD_WIRE_FORMAT_H

#include "protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    SWIPTERMD_STAT_WIRE_SIZE = 88,
    SWIPTERMD_MIRROR_ENTRY_WIRE_SIZE = 20,
    SWIPTERMD_HELLO_REQ_SIZE = 16,
    SWIPTERMD_ERR_REPLY_SIZE = 4,
    SWIPTERMD_HALT_REQ_SIZE = 4,
    SWIPTERMD_HALT_NOTIFY_SIZE = 8,
    SWIPTERMD_RESUME_REQ_SIZE = 8,
    SWIPTERMD_READY_NOTIFY_SIZE = 4,
    SWIPTERMD_STOPPED_NOTIFY_SIZE = 4,
    SWIPTERMD_SPAWN_REPLY_SIZE = 8,
    SWIPTERMD_PROCESS_EXIT_SIZE = 12,
    SWIPTERMD_PROCESS_SIGNAL_REQ_SIZE = 9,
    SWIPTERMD_PTY_CREATE_REQ_SIZE = 16,
    SWIPTERMD_PTY_CREATE_REPLY_SIZE = 8,
    SWIPTERMD_PTY_RESIZE_REQ_SIZE = 8,
    SWIPTERMD_PTY_CLOSE_REQ_SIZE = 4,
    SWIPTERMD_CMD_CANCEL_SIZE = 8,
    SWIPTERMD_CMD_FINISH_SIZE = 8,
    SWIPTERMD_DEV_OPEN_REQ_SIZE = 8,
    SWIPTERMD_DEV_OPEN_REPLY_SIZE = 12,
    SWIPTERMD_DEV_READ_REQ_SIZE = 16,
    SWIPTERMD_DEV_WRITE_REPLY_SIZE = 8,
    SWIPTERMD_DEV_CLOSE_REQ_SIZE = 12,
    SWIPTERMD_DISK_ATTACH_REQ_SIZE = 9,
    SWIPTERMD_DISK_DETACH_REQ_SIZE = 4,
};

enum {
    SWIPTERMD_FS_WRITE_FLAG_APPEND = 1U << 0,
    SWIPTERMD_FS_WRITE_FLAG_SYNC = 1U << 1,
};

enum {
    SWIPTERMD_HALT_REASON_EXITED = 0,
    SWIPTERMD_HALT_REASON_REBOOT = 1,
    SWIPTERMD_HALT_REASON_POWEROFF = 2,
};

typedef struct swiptermd_buf_w {
    uint8_t *data;
    size_t cap;
    size_t len;
    bool overflow;
} swiptermd_buf_w_t;

typedef struct swiptermd_buf_r {
    const uint8_t *data;
    size_t len;
    size_t pos;
    bool underflow;
} swiptermd_buf_r_t;

typedef struct swiptermd_slice {
    const uint8_t *ptr;
    uint32_t len;
} swiptermd_slice_t;

static inline swiptermd_buf_w_t swiptermd_buf_w_init(uint8_t *data, size_t cap) {
    swiptermd_buf_w_t w = {data, cap, 0, false};
    return w;
}

static inline swiptermd_buf_r_t swiptermd_buf_r_init(const uint8_t *data, size_t len) {
    swiptermd_buf_r_t r = {data, len, 0, false};
    return r;
}

static inline bool swiptermd_w_reserve(swiptermd_buf_w_t *w, size_t n) {
    if (w->overflow || n > w->cap - w->len) {
        w->overflow = true;
        return false;
    }
    return true;
}

static inline void swiptermd_w_u8(swiptermd_buf_w_t *w, uint8_t v) {
    if (!swiptermd_w_reserve(w, 1))
        return;
    w->data[w->len++] = v;
}

static inline void swiptermd_w_u16(swiptermd_buf_w_t *w, uint16_t v) {
    if (!swiptermd_w_reserve(w, 2))
        return;
    swiptermd_put_be16(w->data + w->len, v);
    w->len += 2;
}

static inline void swiptermd_w_u32(swiptermd_buf_w_t *w, uint32_t v) {
    if (!swiptermd_w_reserve(w, 4))
        return;
    swiptermd_put_be32(w->data + w->len, v);
    w->len += 4;
}

static inline void swiptermd_w_i32(swiptermd_buf_w_t *w, int32_t v) {
    swiptermd_w_u32(w, (uint32_t)v);
}

static inline void swiptermd_w_u64(swiptermd_buf_w_t *w, uint64_t v) {
    if (!swiptermd_w_reserve(w, 8))
        return;
    swiptermd_put_be64(w->data + w->len, v);
    w->len += 8;
}

static inline void swiptermd_w_i64(swiptermd_buf_w_t *w, int64_t v) {
    swiptermd_w_u64(w, (uint64_t)v);
}

static inline void swiptermd_w_raw(swiptermd_buf_w_t *w, const void *src, size_t len) {
    if (!swiptermd_w_reserve(w, len))
        return;
    if (len > 0)
        memcpy(w->data + w->len, src, len);
    w->len += len;
}

static inline void swiptermd_w_bytes(swiptermd_buf_w_t *w, const void *src, uint32_t len) {
    swiptermd_w_u32(w, len);
    swiptermd_w_raw(w, src, len);
}

static inline void swiptermd_w_str(swiptermd_buf_w_t *w, const char *s) {
    uint32_t len = s ? (uint32_t)strlen(s) : 0;
    swiptermd_w_bytes(w, s ? (const void *)s : (const void *)"", len);
}

static inline void swiptermd_w_strv(swiptermd_buf_w_t *w, const char *const *vec, uint32_t count) {
    swiptermd_w_u32(w, count);
    for (uint32_t i = 0; i < count; i++)
        swiptermd_w_str(w, vec[i]);
}

static inline bool swiptermd_r_avail(swiptermd_buf_r_t *r, size_t n) {
    if (r->underflow || n > r->len - r->pos) {
        r->underflow = true;
        return false;
    }
    return true;
}

static inline uint8_t swiptermd_r_u8(swiptermd_buf_r_t *r) {
    if (!swiptermd_r_avail(r, 1))
        return 0;
    return r->data[r->pos++];
}

static inline uint16_t swiptermd_r_u16(swiptermd_buf_r_t *r) {
    if (!swiptermd_r_avail(r, 2))
        return 0;
    uint16_t v = swiptermd_get_be16(r->data + r->pos);
    r->pos += 2;
    return v;
}

static inline uint32_t swiptermd_r_u32(swiptermd_buf_r_t *r) {
    if (!swiptermd_r_avail(r, 4))
        return 0;
    uint32_t v = swiptermd_get_be32(r->data + r->pos);
    r->pos += 4;
    return v;
}

static inline int32_t swiptermd_r_i32(swiptermd_buf_r_t *r) {
    return (int32_t)swiptermd_r_u32(r);
}

static inline uint64_t swiptermd_r_u64(swiptermd_buf_r_t *r) {
    if (!swiptermd_r_avail(r, 8))
        return 0;
    uint64_t v = swiptermd_get_be64(r->data + r->pos);
    r->pos += 8;
    return v;
}

static inline int64_t swiptermd_r_i64(swiptermd_buf_r_t *r) {
    return (int64_t)swiptermd_r_u64(r);
}

static inline void swiptermd_r_raw(swiptermd_buf_r_t *r, void *dst, size_t len) {
    if (!swiptermd_r_avail(r, len))
        return;
    if (len > 0)
        memcpy(dst, r->data + r->pos, len);
    r->pos += len;
}

static inline swiptermd_slice_t swiptermd_r_bytes(swiptermd_buf_r_t *r) {
    swiptermd_slice_t out = {NULL, 0};
    uint32_t len = swiptermd_r_u32(r);
    if (!swiptermd_r_avail(r, len))
        return out;
    out.ptr = r->data + r->pos;
    out.len = len;
    r->pos += len;
    return out;
}

static inline bool swiptermd_r_str_copy(swiptermd_buf_r_t *r, char *dst, size_t dst_cap) {
    swiptermd_slice_t s = swiptermd_r_bytes(r);
    if (r->underflow || (size_t)s.len + 1 > dst_cap) {
        r->underflow = true;
        return false;
    }
    if (s.len > 0)
        memcpy(dst, s.ptr, s.len);
    dst[s.len] = '\0';
    return true;
}

static inline void swiptermd_stat_encode(swiptermd_buf_w_t *w, const swiptermd_stat_wire_t *st) {
    swiptermd_w_u32(w, st->mode);
    swiptermd_w_u32(w, st->uid);
    swiptermd_w_u32(w, st->gid);
    swiptermd_w_u32(w, st->nlink);
    swiptermd_w_u64(w, st->size);
    swiptermd_w_u64(w, st->inode);
    swiptermd_w_u64(w, st->rdev);
    swiptermd_w_i64(w, st->atime_sec);
    swiptermd_w_i64(w, st->atime_nsec);
    swiptermd_w_i64(w, st->mtime_sec);
    swiptermd_w_i64(w, st->mtime_nsec);
    swiptermd_w_i64(w, st->ctime_sec);
    swiptermd_w_i64(w, st->ctime_nsec);
}

static inline void swiptermd_stat_decode(swiptermd_buf_r_t *r, swiptermd_stat_wire_t *st) {
    st->mode = swiptermd_r_u32(r);
    st->uid = swiptermd_r_u32(r);
    st->gid = swiptermd_r_u32(r);
    st->nlink = swiptermd_r_u32(r);
    st->size = swiptermd_r_u64(r);
    st->inode = swiptermd_r_u64(r);
    st->rdev = swiptermd_r_u64(r);
    st->atime_sec = swiptermd_r_i64(r);
    st->atime_nsec = swiptermd_r_i64(r);
    st->mtime_sec = swiptermd_r_i64(r);
    st->mtime_nsec = swiptermd_r_i64(r);
    st->ctime_sec = swiptermd_r_i64(r);
    st->ctime_nsec = swiptermd_r_i64(r);
}

static inline void swiptermd_mirror_entry_encode(swiptermd_buf_w_t *w,
                                                 const swiptermd_mirror_entry_t *e) {
    swiptermd_w_u8(w, e->family);
    swiptermd_w_u8(w, e->action);
    swiptermd_w_u16(w, e->port);
    swiptermd_w_raw(w, e->addr, sizeof(e->addr));
}

static inline void swiptermd_mirror_entry_decode(swiptermd_buf_r_t *r,
                                                 swiptermd_mirror_entry_t *e) {
    e->family = swiptermd_r_u8(r);
    e->action = swiptermd_r_u8(r);
    e->port = swiptermd_r_u16(r);
    swiptermd_r_raw(r, e->addr, sizeof(e->addr));
}

static inline uint32_t swiptermd_mirror_pack_resource_id(uint8_t family, uint16_t port) {
    return ((uint32_t)family << 16) | (uint32_t)port;
}

static inline void swiptermd_mirror_unpack_resource_id(uint32_t resource_id,
                                                       uint8_t *family_out,
                                                       uint16_t *port_out) {
    *family_out = (uint8_t)(resource_id >> 16);
    *port_out = (uint16_t)(resource_id & 0xffffU);
}

static inline size_t swiptermd_enc_hello(uint8_t out[SWIPTERMD_HELLO_REQ_SIZE],
                                         uint64_t session_nonce, uint32_t caps) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_HELLO_REQ_SIZE);
    swiptermd_w_u64(&w, session_nonce);
    swiptermd_w_u32(&w, caps);
    swiptermd_w_u32(&w, 0);
    return w.len;
}

static inline bool swiptermd_dec_hello(const uint8_t *in, size_t len,
                                       uint64_t *session_nonce, uint32_t *caps) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *session_nonce = swiptermd_r_u64(&r);
    *caps = swiptermd_r_u32(&r);
    (void)swiptermd_r_u32(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_err(uint8_t out[SWIPTERMD_ERR_REPLY_SIZE], int32_t err) {
    swiptermd_put_be32(out, (uint32_t)err);
    return SWIPTERMD_ERR_REPLY_SIZE;
}

static inline bool swiptermd_dec_err(const uint8_t *in, size_t len, int32_t *err) {
    if (len < SWIPTERMD_ERR_REPLY_SIZE)
        return false;
    *err = (int32_t)swiptermd_get_be32(in);
    return true;
}

static inline bool swiptermd_enc_configure_init(swiptermd_buf_w_t *w, uint32_t init_mode,
                                                const char *hostname, const char *host_commands_dir) {
    swiptermd_w_u32(w, init_mode);
    swiptermd_w_str(w, hostname);
    swiptermd_w_str(w, host_commands_dir);
    return !w->overflow;
}

static inline bool swiptermd_enc_start_init(swiptermd_buf_w_t *w, uint32_t init_mode,
                                            uint64_t console_token, uint8_t has_console,
                                            const char *path,
                                            const char *const *argv, uint32_t argc,
                                            const char *const *envp, uint32_t envc) {
    swiptermd_w_u32(w, init_mode);
    swiptermd_w_u64(w, console_token);
    swiptermd_w_u8(w, has_console);
    swiptermd_w_str(w, path);
    swiptermd_w_strv(w, argv, argc);
    swiptermd_w_strv(w, envp, envc);
    return !w->overflow;
}

static inline size_t swiptermd_enc_halt_req(uint8_t out[SWIPTERMD_HALT_REQ_SIZE], uint32_t flags) {
    swiptermd_put_be32(out, flags);
    return SWIPTERMD_HALT_REQ_SIZE;
}

static inline bool swiptermd_dec_halt_req(const uint8_t *in, size_t len, uint32_t *flags) {
    if (len < SWIPTERMD_HALT_REQ_SIZE)
        return false;
    *flags = swiptermd_get_be32(in);
    return true;
}

static inline size_t swiptermd_enc_halt_notify(uint8_t out[SWIPTERMD_HALT_NOTIFY_SIZE],
                                               uint32_t reason, int32_t status) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_HALT_NOTIFY_SIZE);
    swiptermd_w_u32(&w, reason);
    swiptermd_w_i32(&w, status);
    return w.len;
}

static inline bool swiptermd_dec_halt_notify(const uint8_t *in, size_t len,
                                             uint32_t *reason, int32_t *status) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *reason = swiptermd_r_u32(&r);
    *status = swiptermd_r_i32(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_resume_req(uint8_t out[SWIPTERMD_RESUME_REQ_SIZE], int64_t realtime_ns) {
    swiptermd_put_be64(out, (uint64_t)realtime_ns);
    return SWIPTERMD_RESUME_REQ_SIZE;
}

static inline bool swiptermd_dec_resume_req(const uint8_t *buf, size_t len, int64_t *realtime_ns) {
    if (len < SWIPTERMD_RESUME_REQ_SIZE)
        return false;
    *realtime_ns = (int64_t)swiptermd_get_be64(buf);
    return true;
}

static inline size_t swiptermd_enc_u32_notify(uint8_t out[4], uint32_t value) {
    swiptermd_put_be32(out, value);
    return 4;
}

static inline bool swiptermd_dec_u32_notify(const uint8_t *in, size_t len, uint32_t *value) {
    if (len < 4)
        return false;
    *value = swiptermd_get_be32(in);
    return true;
}

static inline bool swiptermd_enc_login_req(swiptermd_buf_w_t *w, uint32_t process_id, uint32_t pty_id,
                                           const char *user, const char *const *envp, uint32_t envc) {
    swiptermd_w_u32(w, process_id);
    swiptermd_w_u32(w, pty_id);
    swiptermd_w_str(w, user);
    swiptermd_w_strv(w, envp, envc);
    return !w->overflow;
}

static inline bool swiptermd_enc_spawn_req(swiptermd_buf_w_t *w, uint32_t process_id,
                                           const uint8_t stdio_mode[3],
                                           const uint32_t stdio_pty_id[3],
                                           const uint64_t stdio_token[3],
                                           const char *path, const char *working_dir,
                                           const char *const *argv, uint32_t argc,
                                           const char *const *envp, uint32_t envc) {
    swiptermd_w_u32(w, process_id);
    swiptermd_w_u8(w, stdio_mode[0]);
    swiptermd_w_u8(w, stdio_mode[1]);
    swiptermd_w_u8(w, stdio_mode[2]);
    swiptermd_w_u8(w, 0);
    for (int i = 0; i < 3; i++)
        swiptermd_w_u32(w, stdio_pty_id[i]);
    for (int i = 0; i < 3; i++)
        swiptermd_w_u64(w, stdio_token[i]);
    swiptermd_w_str(w, path);
    swiptermd_w_str(w, working_dir);
    swiptermd_w_strv(w, argv, argc);
    swiptermd_w_strv(w, envp, envc);
    return !w->overflow;
}

static inline size_t swiptermd_enc_disk_attach_req(uint8_t out[SWIPTERMD_DISK_ATTACH_REQ_SIZE], uint32_t disk_id,
                                                   uint32_t slot, bool read_only) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_DISK_ATTACH_REQ_SIZE);
    swiptermd_w_u32(&w, disk_id);
    swiptermd_w_u32(&w, slot);
    swiptermd_w_u8(&w, read_only ? 1 : 0);
    return w.len;
}

static inline bool swiptermd_dec_disk_attach_req(const uint8_t *in, size_t len, uint32_t *disk_id, uint32_t *slot,
                                                 bool *read_only) {
    if (len < SWIPTERMD_DISK_ATTACH_REQ_SIZE)
        return false;
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *disk_id = swiptermd_r_u32(&r);
    *slot = swiptermd_r_u32(&r);
    *read_only = swiptermd_r_u8(&r) != 0;
    return true;
}

static inline bool swiptermd_enc_disk_path_req(swiptermd_buf_w_t *w, uint32_t disk_id, const char *guest_path) {
    swiptermd_w_u32(w, disk_id);
    swiptermd_w_str(w, guest_path);
    return !w->overflow;
}

static inline bool swiptermd_enc_disk_spawn_prefix(swiptermd_buf_w_t *w, uint32_t disk_id, uint32_t invocation_id,
                                                   uint32_t flags, const char *hostname) {
    swiptermd_w_u32(w, disk_id);
    swiptermd_w_u32(w, invocation_id);
    swiptermd_w_u32(w, flags);
    swiptermd_w_str(w, hostname);
    return !w->overflow;
}

static inline size_t swiptermd_enc_spawn_reply(uint8_t out[SWIPTERMD_SPAWN_REPLY_SIZE],
                                               int32_t err, int32_t pid) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_SPAWN_REPLY_SIZE);
    swiptermd_w_i32(&w, err);
    swiptermd_w_i32(&w, pid);
    return w.len;
}

static inline bool swiptermd_dec_spawn_reply(const uint8_t *in, size_t len,
                                             int32_t *err, int32_t *pid) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *err = swiptermd_r_i32(&r);
    *pid = swiptermd_r_i32(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_process_exit(uint8_t out[SWIPTERMD_PROCESS_EXIT_SIZE],
                                                uint32_t process_id, int32_t pid, int32_t status) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_PROCESS_EXIT_SIZE);
    swiptermd_w_u32(&w, process_id);
    swiptermd_w_i32(&w, pid);
    swiptermd_w_i32(&w, status);
    return w.len;
}

static inline bool swiptermd_dec_process_exit(const uint8_t *in, size_t len,
                                              uint32_t *process_id, int32_t *pid, int32_t *status) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *process_id = swiptermd_r_u32(&r);
    *pid = swiptermd_r_i32(&r);
    *status = swiptermd_r_i32(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_process_signal(uint8_t out[SWIPTERMD_PROCESS_SIGNAL_REQ_SIZE],
                                                  uint32_t process_id, int32_t sig, bool to_group) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_PROCESS_SIGNAL_REQ_SIZE);
    swiptermd_w_u32(&w, process_id);
    swiptermd_w_i32(&w, sig);
    swiptermd_w_u8(&w, to_group ? 1 : 0);
    return w.len;
}

static inline bool swiptermd_dec_process_signal(const uint8_t *in, size_t len,
                                                uint32_t *process_id, int32_t *sig, bool *to_group) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *process_id = swiptermd_r_u32(&r);
    *sig = swiptermd_r_i32(&r);
    *to_group = swiptermd_r_u8(&r) != 0;
    return !r.underflow;
}

static inline size_t swiptermd_enc_pty_create_req(uint8_t out[SWIPTERMD_PTY_CREATE_REQ_SIZE],
                                                  uint32_t pty_id, uint16_t cols, uint16_t rows,
                                                  uint64_t stream_token) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_PTY_CREATE_REQ_SIZE);
    swiptermd_w_u32(&w, pty_id);
    swiptermd_w_u16(&w, cols);
    swiptermd_w_u16(&w, rows);
    swiptermd_w_u64(&w, stream_token);
    return w.len;
}

static inline bool swiptermd_dec_pty_create_req(const uint8_t *in, size_t len,
                                                uint32_t *pty_id, uint16_t *cols, uint16_t *rows,
                                                uint64_t *stream_token) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *pty_id = swiptermd_r_u32(&r);
    *cols = swiptermd_r_u16(&r);
    *rows = swiptermd_r_u16(&r);
    *stream_token = swiptermd_r_u64(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_pty_create_reply(uint8_t out[SWIPTERMD_PTY_CREATE_REPLY_SIZE],
                                                    int32_t err, uint32_t pts_num) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_PTY_CREATE_REPLY_SIZE);
    swiptermd_w_i32(&w, err);
    swiptermd_w_u32(&w, pts_num);
    return w.len;
}

static inline bool swiptermd_dec_pty_create_reply(const uint8_t *in, size_t len,
                                                  int32_t *err, uint32_t *pts_num) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *err = swiptermd_r_i32(&r);
    *pts_num = swiptermd_r_u32(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_pty_resize(uint8_t out[SWIPTERMD_PTY_RESIZE_REQ_SIZE],
                                              uint32_t pty_id, uint16_t cols, uint16_t rows) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_PTY_RESIZE_REQ_SIZE);
    swiptermd_w_u32(&w, pty_id);
    swiptermd_w_u16(&w, cols);
    swiptermd_w_u16(&w, rows);
    return w.len;
}

static inline bool swiptermd_dec_pty_resize(const uint8_t *in, size_t len,
                                            uint32_t *pty_id, uint16_t *cols, uint16_t *rows) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *pty_id = swiptermd_r_u32(&r);
    *cols = swiptermd_r_u16(&r);
    *rows = swiptermd_r_u16(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_pty_close(uint8_t out[SWIPTERMD_PTY_CLOSE_REQ_SIZE],
                                             uint32_t pty_id) {
    swiptermd_put_be32(out, pty_id);
    return SWIPTERMD_PTY_CLOSE_REQ_SIZE;
}

static inline bool swiptermd_dec_pty_close(const uint8_t *in, size_t len, uint32_t *pty_id) {
    if (len < SWIPTERMD_PTY_CLOSE_REQ_SIZE)
        return false;
    *pty_id = swiptermd_get_be32(in);
    return true;
}

static inline bool swiptermd_enc_fs_stat_req(swiptermd_buf_w_t *w, bool follow_symlinks,
                                             const char *path) {
    swiptermd_w_u8(w, follow_symlinks ? 1 : 0);
    swiptermd_w_str(w, path);
    return !w->overflow;
}

static inline bool swiptermd_enc_fs_read_req(swiptermd_buf_w_t *w, const char *path,
                                             uint64_t offset, uint32_t max_len) {
    swiptermd_w_str(w, path);
    swiptermd_w_u64(w, offset);
    swiptermd_w_u32(w, max_len);
    return !w->overflow;
}

static inline bool swiptermd_dec_fs_read_reply(const uint8_t *in, size_t len,
                                               int32_t *err, bool *eof,
                                               swiptermd_slice_t *data) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *err = swiptermd_r_i32(&r);
    if (r.underflow)
        return false;
    if (*err < 0) {
        *eof = true;
        data->ptr = NULL;
        data->len = 0;
        return true;
    }
    *eof = swiptermd_r_u8(&r) != 0;
    *data = swiptermd_r_bytes(&r);
    return !r.underflow;
}

static inline void swiptermd_fs_list_entry_encode(swiptermd_buf_w_t *w,
                                                  uint32_t mode, const char *name) {
    swiptermd_w_u32(w, mode);
    swiptermd_w_str(w, name);
}

static inline bool swiptermd_fs_list_entry_decode(swiptermd_buf_r_t *r,
                                                  uint32_t *mode, swiptermd_slice_t *name) {
    *mode = swiptermd_r_u32(r);
    *name = swiptermd_r_bytes(r);
    return !r->underflow;
}

static inline bool swiptermd_enc_fs_write_req(swiptermd_buf_w_t *w, uint32_t mode, uint32_t flags,
                                              const char *path, const void *data, uint32_t len) {
    swiptermd_w_u32(w, mode);
    swiptermd_w_u32(w, flags);
    swiptermd_w_str(w, path);
    swiptermd_w_bytes(w, data, len);
    return !w->overflow;
}

static inline bool swiptermd_enc_fs_mkdir_req(swiptermd_buf_w_t *w, uint32_t mode,
                                              bool intermediates, const char *path) {
    swiptermd_w_u32(w, mode);
    swiptermd_w_u8(w, intermediates ? 1 : 0);
    swiptermd_w_str(w, path);
    return !w->overflow;
}

static inline bool swiptermd_enc_fs_remove_req(swiptermd_buf_w_t *w, bool recursive,
                                               const char *path) {
    swiptermd_w_u8(w, recursive ? 1 : 0);
    swiptermd_w_str(w, path);
    return !w->overflow;
}

static inline bool swiptermd_enc_fs_mount_virtiofs_req(swiptermd_buf_w_t *w, bool read_only,
                                                       bool is_directory, const char *tag,
                                                       const char *guest_path) {
    swiptermd_w_u8(w, read_only ? 1 : 0);
    swiptermd_w_u8(w, is_directory ? 1 : 0);
    swiptermd_w_str(w, tag);
    swiptermd_w_str(w, guest_path);
    return !w->overflow;
}

static inline bool swiptermd_enc_cmd_start(swiptermd_buf_w_t *w, uint32_t invocation_id,
                                           uint64_t stream_token, const char *name,
                                           const char *working_dir,
                                           const char *const *argv, uint32_t argc,
                                           const char *const *envp, uint32_t envc) {
    swiptermd_w_u32(w, invocation_id);
    swiptermd_w_u64(w, stream_token);
    swiptermd_w_str(w, name);
    swiptermd_w_str(w, working_dir);
    swiptermd_w_strv(w, argv, argc);
    swiptermd_w_strv(w, envp, envc);
    return !w->overflow;
}

static inline size_t swiptermd_enc_cmd_cancel(uint8_t out[SWIPTERMD_CMD_CANCEL_SIZE],
                                              uint32_t invocation_id, int32_t sig) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_CMD_CANCEL_SIZE);
    swiptermd_w_u32(&w, invocation_id);
    swiptermd_w_i32(&w, sig);
    return w.len;
}

static inline bool swiptermd_dec_cmd_cancel(const uint8_t *in, size_t len,
                                            uint32_t *invocation_id, int32_t *sig) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *invocation_id = swiptermd_r_u32(&r);
    *sig = swiptermd_r_i32(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_cmd_finish(uint8_t out[SWIPTERMD_CMD_FINISH_SIZE],
                                              uint32_t invocation_id, int32_t status) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_CMD_FINISH_SIZE);
    swiptermd_w_u32(&w, invocation_id);
    swiptermd_w_i32(&w, status);
    return w.len;
}

static inline bool swiptermd_dec_cmd_finish(const uint8_t *in, size_t len,
                                            uint32_t *invocation_id, int32_t *status) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *invocation_id = swiptermd_r_u32(&r);
    *status = swiptermd_r_i32(&r);
    return !r.underflow;
}

static inline bool swiptermd_enc_register_device(swiptermd_buf_w_t *w, uint32_t device_id,
                                                 const char *path) {
    swiptermd_w_u32(w, device_id);
    swiptermd_w_str(w, path);
    return !w->overflow;
}

static inline size_t swiptermd_enc_dev_open_req(uint8_t out[SWIPTERMD_DEV_OPEN_REQ_SIZE],
                                                uint32_t device_id, uint32_t flags) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_DEV_OPEN_REQ_SIZE);
    swiptermd_w_u32(&w, device_id);
    swiptermd_w_u32(&w, flags);
    return w.len;
}

static inline bool swiptermd_dec_dev_open_req(const uint8_t *in, size_t len,
                                              uint32_t *device_id, uint32_t *flags) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *device_id = swiptermd_r_u32(&r);
    *flags = swiptermd_r_u32(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_dev_open_reply(uint8_t out[SWIPTERMD_DEV_OPEN_REPLY_SIZE],
                                                  int32_t err, uint64_t fh) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_DEV_OPEN_REPLY_SIZE);
    swiptermd_w_i32(&w, err);
    swiptermd_w_u64(&w, fh);
    return w.len;
}

static inline bool swiptermd_dec_dev_open_reply(const uint8_t *in, size_t len,
                                                int32_t *err, uint64_t *fh) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *err = swiptermd_r_i32(&r);
    *fh = swiptermd_r_u64(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_dev_read_req(uint8_t out[SWIPTERMD_DEV_READ_REQ_SIZE],
                                                uint32_t device_id, uint64_t fh,
                                                uint32_t max_len) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_DEV_READ_REQ_SIZE);
    swiptermd_w_u32(&w, device_id);
    swiptermd_w_u64(&w, fh);
    swiptermd_w_u32(&w, max_len);
    return w.len;
}

static inline bool swiptermd_dec_dev_read_req(const uint8_t *in, size_t len,
                                              uint32_t *device_id, uint64_t *fh,
                                              uint32_t *max_len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *device_id = swiptermd_r_u32(&r);
    *fh = swiptermd_r_u64(&r);
    *max_len = swiptermd_r_u32(&r);
    return !r.underflow;
}

static inline bool swiptermd_enc_dev_read_reply(swiptermd_buf_w_t *w, int32_t err,
                                                const void *data, uint32_t len) {
    swiptermd_w_i32(w, err);
    if (err >= 0)
        swiptermd_w_bytes(w, data, len);
    return !w->overflow;
}

static inline bool swiptermd_dec_dev_read_reply(const uint8_t *in, size_t len,
                                                int32_t *err, swiptermd_slice_t *data) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *err = swiptermd_r_i32(&r);
    if (r.underflow)
        return false;
    if (*err < 0) {
        data->ptr = NULL;
        data->len = 0;
        return true;
    }
    *data = swiptermd_r_bytes(&r);
    return !r.underflow;
}

static inline bool swiptermd_enc_dev_write_req(swiptermd_buf_w_t *w, uint32_t device_id,
                                               uint64_t fh, const void *data, uint32_t len) {
    swiptermd_w_u32(w, device_id);
    swiptermd_w_u64(w, fh);
    swiptermd_w_bytes(w, data, len);
    return !w->overflow;
}

static inline bool swiptermd_dec_dev_write_req(const uint8_t *in, size_t len,
                                               uint32_t *device_id, uint64_t *fh,
                                               swiptermd_slice_t *data) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *device_id = swiptermd_r_u32(&r);
    *fh = swiptermd_r_u64(&r);
    *data = swiptermd_r_bytes(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_dev_write_reply(uint8_t out[SWIPTERMD_DEV_WRITE_REPLY_SIZE],
                                                   int32_t err, uint32_t written) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_DEV_WRITE_REPLY_SIZE);
    swiptermd_w_i32(&w, err);
    swiptermd_w_u32(&w, written);
    return w.len;
}

static inline bool swiptermd_dec_dev_write_reply(const uint8_t *in, size_t len,
                                                 int32_t *err, uint32_t *written) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *err = swiptermd_r_i32(&r);
    *written = swiptermd_r_u32(&r);
    return !r.underflow;
}

static inline size_t swiptermd_enc_dev_close_req(uint8_t out[SWIPTERMD_DEV_CLOSE_REQ_SIZE],
                                                 uint32_t device_id, uint64_t fh) {
    swiptermd_buf_w_t w = swiptermd_buf_w_init(out, SWIPTERMD_DEV_CLOSE_REQ_SIZE);
    swiptermd_w_u32(&w, device_id);
    swiptermd_w_u64(&w, fh);
    return w.len;
}

static inline bool swiptermd_dec_dev_close_req(const uint8_t *in, size_t len,
                                               uint32_t *device_id, uint64_t *fh) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(in, len);
    *device_id = swiptermd_r_u32(&r);
    *fh = swiptermd_r_u64(&r);
    return !r.underflow;
}

#endif
