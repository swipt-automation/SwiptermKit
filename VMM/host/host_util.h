#ifndef VMM_HOST_UTIL_H
#define VMM_HOST_UTIL_H

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

enum {
    MODULE_NAME_MAX = 63,
    MODULE_FILE_LIMIT = 64 << 20,
    KERNEL_IMAGE_LIMIT = 256 << 20,
};

static const char *const MODULE_BASES[] = {"/lib/modules", "/usr/lib/modules"};
static const uint8_t XZ_MAGIC[6] = {0xFD, '7', 'z', 'X', 'Z', 0x00};

static inline uint16_t get_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t get_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint64_t get_le64(const uint8_t *p) {
    return (uint64_t)get_le32(p) | ((uint64_t)get_le32(p + 4) << 32);
}

static inline uint16_t get_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t get_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline uint64_t get_be64(const uint8_t *p) {
    return ((uint64_t)get_be32(p) << 32) | get_be32(p + 4);
}

static inline void put_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static inline void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline uint32_t fnv1a_32(const char *s, size_t len) {
    uint32_t h = 2166136261U;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)s[i];
        h *= 16777619U;
    }
    return h;
}

static inline bool is_power_of(uint32_t n, uint32_t base) {
    while (n > 1 && n % base == 0) {
        n /= base;
    }
    return n == 1;
}

static inline ssize_t pread_all(int fd, void *buf, size_t len, off_t off) {
    size_t done = 0;
    while (done < len) {
        ssize_t n = pread(fd, (uint8_t *)buf + done, len - done, off + (off_t)done);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0) {
            return -errno;
        }
        if (n == 0) {
            break;
        }
        done += (size_t)n;
    }
    return (ssize_t)done;
}

static inline void set_error(char **error_message, const char *message) {
    if (error_message && !*error_message) {
        *error_message = strdup(message && message[0] ? message : "unknown error");
    }
}

static inline void set_error_detail(char **error_message, const char *what, const char *detail) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s: %s", what, detail && detail[0] ? detail : "unknown error");
    set_error(error_message, buf);
}

static inline size_t module_suffix_len(const char *name, size_t len) {
    static const char *const suffixes[] = {".ko", ".ko.gz", ".ko.xz", ".ko.zst"};
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        size_t n = strlen(suffixes[i]);
        if (len > n && memcmp(name + len - n, suffixes[i], n) == 0) {
            return n;
        }
    }
    return 0;
}

static inline void normalize_module_name(const char *src, size_t len, char *out, size_t out_size) {
    size_t n = len < out_size - 1 ? len : out_size - 1;
    int bracket = 0;
    for (size_t i = 0; i < n; i++) {
        char c = src[i];
        if (c == '[') {
            bracket++;
        } else if (c == ']' && bracket > 0) {
            bracket--;
        }
        out[i] = (c == '-' && bracket == 0) ? '_' : c;
    }
    out[n] = '\0';
}

typedef struct ino_map {
    struct ino_slot {
        uint32_t ino;
        char *path;
    } *slots;
    size_t cap;
    size_t count;
} ino_map_t;

static inline char **ino_map_slot(ino_map_t *m, uint32_t ino, bool *found) {
    if ((m->count + 1) * 2 > m->cap) {
        size_t cap = m->cap ? m->cap * 2 : 256;
        struct ino_slot *slots = calloc(cap, sizeof(*slots));
        if (!slots) {
            return NULL;
        }
        for (size_t i = 0; i < m->cap; i++) {
            if (m->slots[i].ino != 0) {
                size_t pos = (m->slots[i].ino * 2654435761U) & (cap - 1);
                while (slots[pos].ino != 0) {
                    pos = (pos + 1) & (cap - 1);
                }
                slots[pos] = m->slots[i];
            }
        }
        free(m->slots);
        m->slots = slots;
        m->cap = cap;
    }
    size_t pos = (ino * 2654435761U) & (m->cap - 1);
    while (m->slots[pos].ino != 0 && m->slots[pos].ino != ino) {
        pos = (pos + 1) & (m->cap - 1);
    }
    *found = m->slots[pos].ino == ino;
    if (!*found) {
        m->slots[pos].ino = ino;
        m->count++;
    }
    return &m->slots[pos].path;
}

static inline void ino_map_free(ino_map_t *m) {
    for (size_t i = 0; i < m->cap; i++) {
        free(m->slots[i].path);
    }
    free(m->slots);
    *m = (ino_map_t){0};
}

#endif
