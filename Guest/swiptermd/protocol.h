#ifndef SWIPTERMD_PROTOCOL_H
#define SWIPTERMD_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    SWIPTERMD_PROTO_VERSION = 1,
    SWIPTERMD_CONTROL_PORT = 4049,
    SWIPTERMD_DATA_PORT = 4052,
    SWIPTERMD_HOST_CID = 2,
    SWIPTERMD_GUEST_CID = 3,
    SWIPTERMD_FRAME_HEADER_SIZE = 12,
    SWIPTERMD_ATTACH_HEADER_SIZE = 24,
    SWIPTERMD_MAX_BODY_LEN = 16U * 1024U * 1024U,
};

#define SWIPTERMD_OP_REPLY_FLAG 0x8000U

typedef enum swiptermd_opcode {
    SWIPTERMD_OP_HELLO = 0x0001,
    SWIPTERMD_OP_CONFIGURE_INIT = 0x0002,
    SWIPTERMD_OP_START_INIT = 0x0003,
    SWIPTERMD_OP_HALT = 0x0004,
    SWIPTERMD_OP_SUSPEND = 0x0005,
    SWIPTERMD_OP_RESUME = 0x0006,
    SWIPTERMD_OP_READY = 0x0007,
    SWIPTERMD_OP_STOPPED = 0x0008,

    SWIPTERMD_OP_SPAWN = 0x0010,
    SWIPTERMD_OP_PROCESS_EXIT = 0x0011,
    SWIPTERMD_OP_PROCESS_SIGNAL = 0x0012,
    SWIPTERMD_OP_LOGIN = 0x0013,

    SWIPTERMD_OP_PTY_CREATE = 0x0020,
    SWIPTERMD_OP_PTY_RESIZE = 0x0021,
    SWIPTERMD_OP_PTY_CLOSE = 0x0022,

    SWIPTERMD_OP_FS_STAT = 0x0030,
    SWIPTERMD_OP_FS_READ = 0x0031,
    SWIPTERMD_OP_FS_WRITE = 0x0032,
    SWIPTERMD_OP_FS_LIST = 0x0033,
    SWIPTERMD_OP_FS_MKDIR = 0x0034,
    SWIPTERMD_OP_FS_REMOVE = 0x0035,
    SWIPTERMD_OP_FS_MOUNT_VIRTIOFS = 0x0036,
    SWIPTERMD_OP_FS_UNMOUNT = 0x0037,

    SWIPTERMD_OP_REGISTER_CMD = 0x0050,
    SWIPTERMD_OP_CMD_START = 0x0051,
    SWIPTERMD_OP_CMD_CANCEL = 0x0052,
    SWIPTERMD_OP_CMD_FINISH = 0x0053,

    SWIPTERMD_OP_REGISTER_DEVICE = 0x0060,
    SWIPTERMD_OP_DEV_OPEN = 0x0061,
    SWIPTERMD_OP_DEV_READ = 0x0062,
    SWIPTERMD_OP_DEV_WRITE = 0x0063,
    SWIPTERMD_OP_DEV_CLOSE = 0x0064,

    SWIPTERMD_OP_MIRROR_UPDATE = 0x0070,
} swiptermd_opcode_t;

typedef enum swiptermd_stream_role {
    SWIPTERMD_STREAM_STDIO_IN = 1,
    SWIPTERMD_STREAM_STDIO_OUT = 2,
    SWIPTERMD_STREAM_STDIO_ERR = 3,
    SWIPTERMD_STREAM_PTY = 4,
    SWIPTERMD_STREAM_CMD_IN = 5,
    SWIPTERMD_STREAM_CMD_OUT = 6,
    SWIPTERMD_STREAM_CMD_ERR = 7,
    SWIPTERMD_STREAM_CONSOLE = 8,
    SWIPTERMD_STREAM_TCP_MIRROR = 9,
} swiptermd_stream_role_t;

typedef enum swiptermd_capability {
    SWIPTERMD_CAP_READY = 1U << 0,
    SWIPTERMD_CAP_LOGIN = 1U << 1,
    SWIPTERMD_CAP_STOP_STATUS = 1U << 2,
    SWIPTERMD_CAP_DEVICES = 1U << 3,
    SWIPTERMD_CAP_VIRTIOFS = 1U << 4,
} swiptermd_capability_t;

typedef enum swiptermd_ready_level {
    SWIPTERMD_READY_LOGIN = 1,
    SWIPTERMD_READY_SYSTEM = 2,
} swiptermd_ready_level_t;

typedef enum swiptermd_stop_flag {
    SWIPTERMD_STOP_FORCED = 1U << 0,
    SWIPTERMD_STOP_STORAGE_ERROR = 1U << 1,
} swiptermd_stop_flag_t;

typedef enum swiptermd_init_mode {
    SWIPTERMD_INIT_MANAGED = 0,
    SWIPTERMD_INIT_PROCESS = 1,
} swiptermd_init_mode_t;

typedef enum swiptermd_stdio_mode {
    SWIPTERMD_STDIO_NULL = 0,
    SWIPTERMD_STDIO_PIPE = 1,
    SWIPTERMD_STDIO_PTY = 2,
} swiptermd_stdio_mode_t;

typedef enum swiptermd_mirror_family {
    SWIPTERMD_MIRROR_AF_INET = 4,
    SWIPTERMD_MIRROR_AF_INET6 = 6,
} swiptermd_mirror_family_t;

typedef enum swiptermd_mirror_action {
    SWIPTERMD_MIRROR_ADDED = 1,
    SWIPTERMD_MIRROR_REMOVED = 2,
} swiptermd_mirror_action_t;

typedef struct swiptermd_frame_hdr {
    uint32_t body_len;
    uint16_t version;
    uint16_t opcode;
    uint32_t req_id;
} swiptermd_frame_hdr_t;

typedef struct swiptermd_attach_hdr {
    uint64_t session_nonce;
    uint32_t role;
    uint32_t resource_id;
    uint64_t token;
} swiptermd_attach_hdr_t;

typedef struct swiptermd_stat_wire {
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t nlink;
    uint64_t size;
    uint64_t inode;
    uint64_t rdev;
    int64_t atime_sec;
    int64_t atime_nsec;
    int64_t mtime_sec;
    int64_t mtime_nsec;
    int64_t ctime_sec;
    int64_t ctime_nsec;
} swiptermd_stat_wire_t;

typedef struct swiptermd_mirror_entry {
    uint8_t family;
    uint8_t action;
    uint16_t port;
    uint8_t addr[16];
} swiptermd_mirror_entry_t;

static inline void swiptermd_put_be16(uint8_t *dst, uint16_t v) {
    dst[0] = (uint8_t)(v >> 8);
    dst[1] = (uint8_t)v;
}

static inline void swiptermd_put_be32(uint8_t *dst, uint32_t v) {
    dst[0] = (uint8_t)(v >> 24);
    dst[1] = (uint8_t)(v >> 16);
    dst[2] = (uint8_t)(v >> 8);
    dst[3] = (uint8_t)v;
}

static inline void swiptermd_put_be64(uint8_t *dst, uint64_t v) {
    dst[0] = (uint8_t)(v >> 56);
    dst[1] = (uint8_t)(v >> 48);
    dst[2] = (uint8_t)(v >> 40);
    dst[3] = (uint8_t)(v >> 32);
    dst[4] = (uint8_t)(v >> 24);
    dst[5] = (uint8_t)(v >> 16);
    dst[6] = (uint8_t)(v >> 8);
    dst[7] = (uint8_t)v;
}

static inline uint16_t swiptermd_get_be16(const uint8_t *src) {
    return (uint16_t)(((uint16_t)src[0] << 8) | (uint16_t)src[1]);
}

static inline uint32_t swiptermd_get_be32(const uint8_t *src) {
    return ((uint32_t)src[0] << 24) |
           ((uint32_t)src[1] << 16) |
           ((uint32_t)src[2] << 8) |
           (uint32_t)src[3];
}

static inline uint64_t swiptermd_get_be64(const uint8_t *src) {
    return ((uint64_t)src[0] << 56) |
           ((uint64_t)src[1] << 48) |
           ((uint64_t)src[2] << 40) |
           ((uint64_t)src[3] << 32) |
           ((uint64_t)src[4] << 24) |
           ((uint64_t)src[5] << 16) |
           ((uint64_t)src[6] << 8) |
           (uint64_t)src[7];
}

static inline void swiptermd_frame_hdr_encode(uint8_t out[SWIPTERMD_FRAME_HEADER_SIZE],
                                              const swiptermd_frame_hdr_t *hdr) {
    swiptermd_put_be32(out + 0, hdr->body_len);
    swiptermd_put_be16(out + 4, hdr->version);
    swiptermd_put_be16(out + 6, hdr->opcode);
    swiptermd_put_be32(out + 8, hdr->req_id);
}

static inline void swiptermd_frame_hdr_decode(const uint8_t in[SWIPTERMD_FRAME_HEADER_SIZE],
                                              swiptermd_frame_hdr_t *hdr) {
    hdr->body_len = swiptermd_get_be32(in + 0);
    hdr->version = swiptermd_get_be16(in + 4);
    hdr->opcode = swiptermd_get_be16(in + 6);
    hdr->req_id = swiptermd_get_be32(in + 8);
}

static inline void swiptermd_attach_hdr_encode(uint8_t out[SWIPTERMD_ATTACH_HEADER_SIZE],
                                               const swiptermd_attach_hdr_t *hdr) {
    swiptermd_put_be64(out + 0, hdr->session_nonce);
    swiptermd_put_be32(out + 8, hdr->role);
    swiptermd_put_be32(out + 12, hdr->resource_id);
    swiptermd_put_be64(out + 16, hdr->token);
}

static inline void swiptermd_attach_hdr_decode(const uint8_t in[SWIPTERMD_ATTACH_HEADER_SIZE],
                                               swiptermd_attach_hdr_t *hdr) {
    hdr->session_nonce = swiptermd_get_be64(in + 0);
    hdr->role = swiptermd_get_be32(in + 8);
    hdr->resource_id = swiptermd_get_be32(in + 12);
    hdr->token = swiptermd_get_be64(in + 16);
}

#endif
