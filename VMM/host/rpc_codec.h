#ifndef VMM_HOST_RPC_CODEC_H
#define VMM_HOST_RPC_CODEC_H

#include "../../Guest/swiptermd/wire_format.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static inline size_t rpc_str_wire_size(const char *s) {
    return 4 + (s ? strlen(s) : 0);
}

static inline uint32_t rpc_cstr_list_count(const char *const *arr) {
    uint32_t count = 0;
    if (arr) {
        while (arr[count] != NULL) {
            count++;
        }
    }
    return count;
}

static inline size_t rpc_strv_wire_size(const char *const *vec, uint32_t count) {
    size_t total = 4;
    for (uint32_t i = 0; i < count; i++) {
        total += rpc_str_wire_size(vec[i]);
    }
    return total;
}

static inline bool rpc_r_str_dup(swiptermd_buf_r_t *r, char **out) {
    swiptermd_slice_t s = swiptermd_r_bytes(r);
    if (r->underflow) {
        return false;
    }
    char *dst = malloc((size_t)s.len + 1);
    if (!dst) {
        return false;
    }
    if (s.len > 0) {
        memcpy(dst, s.ptr, s.len);
    }
    dst[s.len] = '\0';
    *out = dst;
    return true;
}

static inline void rpc_free_strv(char **arr, size_t count) {
    if (!arr) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}

static inline bool rpc_r_strv_dup(swiptermd_buf_r_t *r, size_t *count_out, char ***arr_out) {
    uint32_t count = swiptermd_r_u32(r);
    if (r->underflow || count > 65536U) {
        return false;
    }
    char **arr = calloc((size_t)count + 1, sizeof(char *));
    if (!arr) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (!rpc_r_str_dup(r, &arr[i])) {
            rpc_free_strv(arr, i);
            return false;
        }
    }
    *count_out = count;
    *arr_out = arr;
    return true;
}

static inline bool rpc_write_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (n == 0) {
            return false;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return true;
}

static inline bool rpc_read_all(int fd, void *buf, size_t len) {
    uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (n == 0) {
            return false;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return true;
}

static inline bool rpc_send_frame(int fd, uint16_t opcode, uint32_t req_id,
                                  const void *body, uint32_t body_len) {
    uint8_t hdr_bytes[SWIPTERMD_FRAME_HEADER_SIZE];
    swiptermd_frame_hdr_t hdr = {
        .body_len = body_len,
        .version = SWIPTERMD_PROTO_VERSION,
        .opcode = opcode,
        .req_id = req_id,
    };
    swiptermd_frame_hdr_encode(hdr_bytes, &hdr);
    if (!rpc_write_all(fd, hdr_bytes, sizeof(hdr_bytes))) {
        return false;
    }
    if (body_len > 0 && !rpc_write_all(fd, body, body_len)) {
        return false;
    }
    return true;
}

static inline bool rpc_recv_frame(int fd, swiptermd_frame_hdr_t *hdr_out, uint8_t **body_out) {
    uint8_t hdr_bytes[SWIPTERMD_FRAME_HEADER_SIZE];
    if (!rpc_read_all(fd, hdr_bytes, sizeof(hdr_bytes))) {
        return false;
    }
    swiptermd_frame_hdr_decode(hdr_bytes, hdr_out);
    if (hdr_out->version != SWIPTERMD_PROTO_VERSION || hdr_out->body_len > SWIPTERMD_MAX_BODY_LEN) {
        return false;
    }
    if (hdr_out->body_len == 0) {
        *body_out = NULL;
        return true;
    }
    uint8_t *body = malloc(hdr_out->body_len);
    if (!body) {
        return false;
    }
    if (!rpc_read_all(fd, body, hdr_out->body_len)) {
        free(body);
        return false;
    }
    *body_out = body;
    return true;
}

#endif
