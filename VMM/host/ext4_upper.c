#define _DARWIN_C_SOURCE

#include "host_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum : uint32_t {
    EXT4_SUPER_OFFSET = 1024,
    EXT4_SUPER_MAGIC = 0xEF53,
    EXT4_ROOT_INO = 2,
    EXT4_FEATURE_COMPAT_HAS_JOURNAL = 0x0004,
    EXT4_FEATURE_INCOMPAT_RECOVER = 0x0004,
    EXT4_FEATURE_INCOMPAT_64BIT = 0x0080,

    EXT4_GOOD_OLD_INODE_SIZE = 128,
    EXT4_N_DIRECT_BLOCKS = 12,
    EXT4_FAST_SYMLINK_MAX = 60,
    EXT4_SYMLINK_MAX = 4096,
    EXT4_EXTENTS_FL = 0x00080000,
    EXT4_INLINE_DATA_FL = 0x10000000,
    EXT4_EXTENT_MAGIC = 0xF30A,
    EXT4_EXTENT_MAX_DEPTH = 5,
    EXT4_EXTENT_INIT_MAX_LEN = 32768,
    EXT4_XATTR_MAGIC = 0xEA020000,
    EXT4_XATTR_BLOCK_HEADER_SIZE = 32,
    EXT4_XATTR_ENTRY_SIZE = 16,
    EXT4_XATTR_VALUE_MAX = 65535,
    EXT4_XATTR_INDEX_USER = 1,
    EXT4_XATTR_INDEX_POSIX_ACL_ACCESS = 2,
    EXT4_XATTR_INDEX_POSIX_ACL_DEFAULT = 3,
    EXT4_XATTR_INDEX_TRUSTED = 4,
    EXT4_XATTR_INDEX_SECURITY = 6,
    EXT4_ACL_VERSION = 1,
    EXT4_ACL_USER = 0x02,
    EXT4_ACL_GROUP = 0x08,
    EXT4_ACL_UNDEFINED_ID = 0xFFFFFFFF,
    POSIX_ACL_XATTR_VERSION = 2,
    UPPER_STREAM_CHUNK = 1024 * 1024,

    JBD2_MAGIC = 0xC03B3998,
    JBD2_DESCRIPTOR_BLOCK = 1,
    JBD2_COMMIT_BLOCK = 2,
    JBD2_SUPERBLOCK_V1 = 3,
    JBD2_SUPERBLOCK_V2 = 4,
    JBD2_REVOKE_BLOCK = 5,
    JBD2_FEATURE_INCOMPAT_REVOKE = 0x01,
    JBD2_FEATURE_INCOMPAT_64BIT = 0x02,
    JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT = 0x04,
    JBD2_FEATURE_INCOMPAT_CSUM_V2 = 0x08,
    JBD2_FEATURE_INCOMPAT_CSUM_V3 = 0x10,
    JBD2_FLAG_ESCAPE = 0x01,
    JBD2_FLAG_SAME_UUID = 0x02,
    JBD2_FLAG_LAST_TAG = 0x08,
    JBD2_HEADER_SIZE = 12,
    JBD2_REVOKE_HEADER_SIZE = 16,
    JBD2_COMMIT_CHECKSUM_OFFSET = 16,
    JBD2_UUID_SIZE = 16,

    CRC32C_POLY = 0x82F63B78,
};

static const uint64_t SWIPTERM_UPPER_SIZE = 64ULL * 1024ULL * 1024ULL * 1024ULL;

static inline void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline uint16_t get_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t get_le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline uint16_t get_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t get_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static inline uint64_t get_be64(const uint8_t *p) {
    return ((uint64_t)get_be32(p) << 32) | get_be32(p + 4);
}

static bool pread_all(int fd, void *buf, size_t len, off_t off) {
    uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = pread(fd, p, len, off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (n == 0) {
            memset(p, 0, len);
            return true;
        }
        p += (size_t)n;
        len -= (size_t)n;
        off += (off_t)n;
    }
    return true;
}

int swipterm_upper_create(const char *path) {
    if (!path) {
        return -LINUX_EINVAL;
    }
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        return -LINUX_EIO;
    }
    bool ok = ftruncate(fd, (off_t)SWIPTERM_UPPER_SIZE) == 0;
    if (close(fd) != 0) {
        ok = false;
    }
    return ok ? 0 : -LINUX_EIO;
}

typedef struct replay_block {
    uint64_t fs_block;
    uint64_t log_block;
    bool escaped;
} replay_block_t;

typedef struct ext4_reader {
    int fd;
    uint32_t block_size;
    uint32_t inodes_per_group;
    uint32_t inode_size;
    uint32_t desc_size;
    uint32_t num_groups;
    uint64_t total_blocks;
    uint32_t incompat;
    uint32_t journal_inum;
    uint8_t *gdt;
    uint8_t *stream_buf;
    replay_block_t *replay;
    size_t replay_count;
} ext4_reader_t;

static size_t replay_lower_bound(const ext4_reader_t *r, uint64_t block) {
    size_t lo = 0;
    size_t hi = r->replay_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (r->replay[mid].fs_block < block) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

static bool ext4_pread(const ext4_reader_t *r, void *buf, size_t len, uint64_t off) {
    if (r->replay_count == 0) {
        return pread_all(r->fd, buf, len, (off_t)off);
    }
    uint8_t *p = buf;
    while (len > 0) {
        uint64_t block = off / r->block_size;
        size_t idx = replay_lower_bound(r, block);
        size_t chunk;
        if (idx == r->replay_count || r->replay[idx].fs_block != block) {
            uint64_t end = idx == r->replay_count ? UINT64_MAX : r->replay[idx].fs_block * r->block_size;
            chunk = end - off < len ? (size_t)(end - off) : len;
            if (!pread_all(r->fd, p, chunk, (off_t)off)) {
                return false;
            }
        } else {
            const replay_block_t *e = &r->replay[idx];
            uint32_t in_block = (uint32_t)(off % r->block_size);
            chunk = r->block_size - in_block < len ? r->block_size - in_block : len;
            if (!pread_all(r->fd, p, chunk, (off_t)(e->log_block * r->block_size + in_block))) {
                return false;
            }
            if (e->escaped && in_block < 4) {
                uint8_t magic[4];
                put_be32(magic, JBD2_MAGIC);
                size_t n = 4 - in_block < chunk ? 4 - in_block : chunk;
                memcpy(p, magic + in_block, n);
            }
        }
        p += chunk;
        off += chunk;
        len -= chunk;
    }
    return true;
}

static int ext4_load(ext4_reader_t *r, bool *blank) {
    uint8_t sb[1024];
    if (!ext4_pread(r, sb, sizeof(sb), EXT4_SUPER_OFFSET)) {
        return -LINUX_EIO;
    }
    if (get_le16(sb + 0x38) != EXT4_SUPER_MAGIC) {
        uint8_t head[4096];
        if (!pread_all(r->fd, head, sizeof(head), 0)) {
            return -LINUX_EIO;
        }
        for (size_t i = 0; i < sizeof(head); i++) {
            if (head[i] != 0) {
                return -LINUX_EINVAL;
            }
        }
        *blank = true;
        return 0;
    }

    uint32_t log_bs = get_le32(sb + 0x18);
    if (log_bs > 6) {
        return -LINUX_EINVAL;
    }
    r->block_size = 1024U << log_bs;
    r->inodes_per_group = get_le32(sb + 0x28);
    r->inode_size = get_le16(sb + 0x58);
    r->incompat = get_le32(sb + 0x60);
    r->desc_size = (r->incompat & EXT4_FEATURE_INCOMPAT_64BIT) ? get_le16(sb + 0xFE) : 32U;
    r->journal_inum = (get_le32(sb + 0x5C) & EXT4_FEATURE_COMPAT_HAS_JOURNAL) ? get_le32(sb + 0xE0) : 0;
    if (r->inode_size < 128 || r->inode_size > 4096 || r->inodes_per_group == 0 || r->desc_size < 32) {
        return -LINUX_EINVAL;
    }
    uint32_t blocks_per_group = get_le32(sb + 0x20);
    if (blocks_per_group == 0) {
        return -LINUX_EINVAL;
    }
    r->total_blocks = get_le32(sb + 0x04);
    if (r->incompat & EXT4_FEATURE_INCOMPAT_64BIT) {
        r->total_blocks |= (uint64_t)get_le32(sb + 0x150) << 32;
    }
    uint64_t num_groups = (r->total_blocks + blocks_per_group - 1) / blocks_per_group;
    if (num_groups == 0 || num_groups > 65536) {
        return -LINUX_EINVAL;
    }
    r->num_groups = (uint32_t)num_groups;

    size_t gdt_bytes = (size_t)r->num_groups * r->desc_size;
    free(r->gdt);
    r->gdt = malloc(gdt_bytes);
    if (!r->gdt) {
        return -LINUX_ENOMEM;
    }
    uint64_t gdt_off = (r->block_size == 1024) ? 2048 : r->block_size;
    if (!ext4_pread(r, r->gdt, gdt_bytes, gdt_off)) {
        return -LINUX_EIO;
    }
    return 0;
}

static bool ext4_read_inode(const ext4_reader_t *r, uint32_t ino, uint8_t *buf) {
    if (ino == 0) {
        return false;
    }
    uint32_t idx = ino - 1;
    uint32_t g = idx / r->inodes_per_group;
    uint32_t local = idx % r->inodes_per_group;
    if (g >= r->num_groups) {
        return false;
    }
    const uint8_t *gd = r->gdt + (size_t)g * r->desc_size;
    uint64_t itable = get_le32(gd + 0x08);
    if (r->desc_size >= 64) {
        itable |= (uint64_t)get_le32(gd + 0x28) << 32;
    }
    return ext4_pread(r, buf, r->inode_size, itable * r->block_size + (uint64_t)local * r->inode_size);
}

typedef struct journal_tag {
    uint64_t fs_block;
    uint64_t log_block;
    uint32_t sequence;
    uint32_t order;
    bool escaped;
} journal_tag_t;

typedef struct journal_revoke {
    uint64_t fs_block;
    uint32_t sequence;
} journal_revoke_t;

typedef struct journal_scan {
    const ext4_reader_t *r;
    uint64_t *log_map;
    uint32_t log_first;
    uint32_t log_last;
    uint32_t incompat;
    uint32_t csum_seed;
    uint32_t crc_table[256];
    journal_tag_t *tags;
    size_t tag_count;
    size_t tag_cap;
    journal_revoke_t *revokes;
    size_t revoke_count;
    size_t revoke_cap;
} journal_scan_t;

static uint32_t crc32c(const uint32_t table[256], uint32_t crc, const uint8_t *p, size_t n) {
    while (n-- > 0) {
        crc = table[(crc ^ *p++) & 0xFFU] ^ (crc >> 8);
    }
    return crc;
}

static bool tid_geq(uint32_t a, uint32_t b) {
    return (int32_t)(a - b) >= 0;
}

static bool journal_map_extents(const ext4_reader_t *r, const uint8_t *node, size_t node_bytes,
                                uint64_t *map, uint32_t count, uint32_t depth_left) {
    if (node_bytes < 12 || get_le16(node) != EXT4_EXTENT_MAGIC) {
        return false;
    }
    uint16_t entries = get_le16(node + 2);
    uint16_t depth = get_le16(node + 6);
    if (12 + (size_t)entries * 12 > node_bytes || depth > depth_left) {
        return false;
    }
    if (depth == 0) {
        for (uint16_t i = 0; i < entries; i++) {
            const uint8_t *e = node + 12 + (size_t)i * 12;
            uint32_t logical = get_le32(e + 0);
            uint16_t raw_len = get_le16(e + 4);
            uint32_t len = raw_len > 32768 ? (uint32_t)(raw_len - 32768) : raw_len;
            uint64_t phys = (uint64_t)get_le32(e + 8) | ((uint64_t)get_le16(e + 6) << 32);
            for (uint32_t k = 0; k < len && logical + k < count; k++) {
                map[logical + k] = phys + k;
            }
        }
        return true;
    }
    uint8_t *child = malloc(r->block_size);
    if (!child) {
        return false;
    }
    bool ok = true;
    for (uint16_t i = 0; i < entries && ok; i++) {
        const uint8_t *idx = node + 12 + (size_t)i * 12;
        uint64_t leaf = (uint64_t)get_le32(idx + 4) | ((uint64_t)get_le16(idx + 8) << 32);
        ok = ext4_pread(r, child, r->block_size, leaf * r->block_size) &&
             journal_map_extents(r, child, r->block_size, map, count, depth - 1);
    }
    free(child);
    return ok;
}

static bool journal_read(const journal_scan_t *s, uint32_t log_block, uint8_t *buf) {
    return ext4_pread(s->r, buf, s->r->block_size, s->log_map[log_block] * s->r->block_size);
}

static uint32_t journal_next(const journal_scan_t *s, uint32_t log_block) {
    log_block++;
    return log_block >= s->log_last ? s->log_first : log_block;
}

static bool journal_tail_valid(const journal_scan_t *s, const uint8_t *buf) {
    static const uint8_t zero[4];
    uint32_t bs = s->r->block_size;
    uint32_t crc = crc32c(s->crc_table, s->csum_seed, buf, bs - 4);
    crc = crc32c(s->crc_table, crc, zero, sizeof(zero));
    return get_be32(buf + bs - 4) == crc;
}

static bool journal_commit_valid(const journal_scan_t *s, const uint8_t *buf) {
    static const uint8_t zero[4];
    uint32_t bs = s->r->block_size;
    uint32_t crc = crc32c(s->crc_table, s->csum_seed, buf, JBD2_COMMIT_CHECKSUM_OFFSET);
    crc = crc32c(s->crc_table, crc, zero, sizeof(zero));
    crc = crc32c(s->crc_table, crc, buf + JBD2_COMMIT_CHECKSUM_OFFSET + 4, bs - JBD2_COMMIT_CHECKSUM_OFFSET - 4);
    return get_be32(buf + JBD2_COMMIT_CHECKSUM_OFFSET) == crc;
}

static bool journal_push_tag(journal_scan_t *s, journal_tag_t tag) {
    if (s->tag_count == s->tag_cap) {
        size_t cap = s->tag_cap ? s->tag_cap * 2 : 256;
        journal_tag_t *tags = realloc(s->tags, cap * sizeof(*tags));
        if (!tags) {
            return false;
        }
        s->tags = tags;
        s->tag_cap = cap;
    }
    s->tags[s->tag_count++] = tag;
    return true;
}

static bool journal_push_revoke(journal_scan_t *s, journal_revoke_t revoke) {
    if (s->revoke_count == s->revoke_cap) {
        size_t cap = s->revoke_cap ? s->revoke_cap * 2 : 64;
        journal_revoke_t *revokes = realloc(s->revokes, cap * sizeof(*revokes));
        if (!revokes) {
            return false;
        }
        s->revokes = revokes;
        s->revoke_cap = cap;
    }
    s->revokes[s->revoke_count++] = revoke;
    return true;
}

static int journal_scan_log(journal_scan_t *s, uint32_t sequence, uint32_t start) {
    uint32_t bs = s->r->block_size;
    bool csum = (s->incompat & (JBD2_FEATURE_INCOMPAT_CSUM_V2 | JBD2_FEATURE_INCOMPAT_CSUM_V3)) != 0;
    bool csum_v3 = (s->incompat & JBD2_FEATURE_INCOMPAT_CSUM_V3) != 0;
    bool wide = (s->incompat & JBD2_FEATURE_INCOMPAT_64BIT) != 0;
    size_t tag_bytes = csum_v3 ? 16 : (size_t)(wide ? 12 : 8) + (csum ? 2 : 0);
    size_t tail_bytes = csum ? 4 : 0;
    size_t record_bytes = wide ? 8 : 4;

    uint8_t *buf = malloc(bs);
    uint8_t *data = malloc(bs);
    if (!buf || !data) {
        free(buf);
        free(data);
        return -LINUX_ENOMEM;
    }

    int rc = 0;
    size_t committed_tags = 0;
    size_t committed_revokes = 0;
    uint32_t order = 0;
    uint32_t block = start;
    uint32_t budget = s->log_last - s->log_first;
    bool end = false;
    while (!end && rc == 0 && budget > 0) {
        budget--;
        if (!journal_read(s, block, buf)) {
            rc = -LINUX_EIO;
            break;
        }
        if (get_be32(buf) != JBD2_MAGIC || get_be32(buf + 8) != sequence) {
            break;
        }
        uint32_t type = get_be32(buf + 4);
        block = journal_next(s, block);
        switch (type) {
        case JBD2_DESCRIPTOR_BLOCK: {
            if (csum && !journal_tail_valid(s, buf)) {
                end = true;
                break;
            }
            size_t off = JBD2_HEADER_SIZE;
            while (off + tag_bytes <= bs - tail_bytes && budget > 0 && rc == 0) {
                const uint8_t *tag = buf + off;
                uint64_t fs_block = get_be32(tag);
                uint32_t flags;
                if (csum_v3) {
                    flags = get_be32(tag + 4);
                    fs_block |= (uint64_t)get_be32(tag + 8) << 32;
                } else {
                    flags = get_be16(tag + 6);
                    if (wide) {
                        fs_block |= (uint64_t)get_be32(tag + 8) << 32;
                    }
                }
                uint32_t log_block = block;
                block = journal_next(s, block);
                budget--;
                bool keep = fs_block < s->r->total_blocks;
                if (keep && csum) {
                    if (!journal_read(s, log_block, data)) {
                        rc = -LINUX_EIO;
                        break;
                    }
                    uint8_t seq_be[4];
                    put_be32(seq_be, sequence);
                    uint32_t crc = crc32c(s->crc_table, s->csum_seed, seq_be, sizeof(seq_be));
                    crc = crc32c(s->crc_table, crc, data, bs);
                    keep = csum_v3 ? get_be32(tag + 12) == crc : get_be16(tag + 4) == (uint16_t)crc;
                }
                if (keep && !journal_push_tag(s, (journal_tag_t){
                        .fs_block = fs_block,
                        .log_block = s->log_map[log_block],
                        .sequence = sequence,
                        .order = order++,
                        .escaped = (flags & JBD2_FLAG_ESCAPE) != 0,
                    })) {
                    rc = -LINUX_ENOMEM;
                    break;
                }
                off += tag_bytes;
                if (!(flags & JBD2_FLAG_SAME_UUID)) {
                    off += JBD2_UUID_SIZE;
                }
                if (flags & JBD2_FLAG_LAST_TAG) {
                    break;
                }
            }
            break;
        }
        case JBD2_COMMIT_BLOCK:
            if (csum && !journal_commit_valid(s, buf)) {
                end = true;
                break;
            }
            committed_tags = s->tag_count;
            committed_revokes = s->revoke_count;
            sequence++;
            break;
        case JBD2_REVOKE_BLOCK: {
            if (csum && !journal_tail_valid(s, buf)) {
                end = true;
                break;
            }
            uint32_t used = get_be32(buf + JBD2_HEADER_SIZE);
            if (used > bs - tail_bytes) {
                end = true;
                break;
            }
            for (size_t off = JBD2_REVOKE_HEADER_SIZE; off + record_bytes <= used; off += record_bytes) {
                uint64_t revoked = wide ? get_be64(buf + off) : get_be32(buf + off);
                if (!journal_push_revoke(s, (journal_revoke_t){.fs_block = revoked, .sequence = sequence})) {
                    rc = -LINUX_ENOMEM;
                    break;
                }
            }
            break;
        }
        default:
            end = true;
            break;
        }
    }
    free(buf);
    free(data);
    s->tag_count = committed_tags;
    s->revoke_count = committed_revokes;
    return rc;
}

static int compare_revokes(const void *a, const void *b) {
    const journal_revoke_t *x = a;
    const journal_revoke_t *y = b;
    return x->fs_block < y->fs_block ? -1 : x->fs_block > y->fs_block;
}

static int compare_tags(const void *a, const void *b) {
    const journal_tag_t *x = a;
    const journal_tag_t *y = b;
    if (x->fs_block != y->fs_block) {
        return x->fs_block < y->fs_block ? -1 : 1;
    }
    return x->order < y->order ? -1 : x->order > y->order;
}

static bool journal_revoked(const journal_scan_t *s, const journal_tag_t *tag) {
    size_t lo = 0;
    size_t hi = s->revoke_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (s->revokes[mid].fs_block < tag->fs_block) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo < s->revoke_count && s->revokes[lo].fs_block == tag->fs_block &&
           tid_geq(s->revokes[lo].sequence, tag->sequence);
}

static int journal_build_replay(ext4_reader_t *r, journal_scan_t *s) {
    qsort(s->revokes, s->revoke_count, sizeof(*s->revokes), compare_revokes);
    size_t unique = 0;
    for (size_t i = 0; i < s->revoke_count; i++) {
        if (unique > 0 && s->revokes[unique - 1].fs_block == s->revokes[i].fs_block) {
            if (!tid_geq(s->revokes[unique - 1].sequence, s->revokes[i].sequence)) {
                s->revokes[unique - 1].sequence = s->revokes[i].sequence;
            }
        } else {
            s->revokes[unique++] = s->revokes[i];
        }
    }
    s->revoke_count = unique;

    size_t live = 0;
    for (size_t i = 0; i < s->tag_count; i++) {
        if (!journal_revoked(s, &s->tags[i])) {
            s->tags[live++] = s->tags[i];
        }
    }
    qsort(s->tags, live, sizeof(*s->tags), compare_tags);

    r->replay = calloc(live > 0 ? live : 1, sizeof(*r->replay));
    if (!r->replay) {
        return -LINUX_ENOMEM;
    }
    size_t count = 0;
    for (size_t i = 0; i < live; i++) {
        if (i + 1 < live && s->tags[i + 1].fs_block == s->tags[i].fs_block) {
            continue;
        }
        r->replay[count++] = (replay_block_t){
            .fs_block = s->tags[i].fs_block,
            .log_block = s->tags[i].log_block,
            .escaped = s->tags[i].escaped,
        };
    }
    r->replay_count = count;
    return 0;
}

static int journal_replay(ext4_reader_t *r) {
    if (r->journal_inum == 0) {
        return -LINUX_EINVAL;
    }
    uint8_t *inode = malloc(r->inode_size);
    uint8_t *jsb = malloc(r->block_size);
    journal_scan_t s = {.r = r};
    int rc = 0;
    if (!inode || !jsb) {
        rc = -LINUX_ENOMEM;
        goto out;
    }
    if (!ext4_read_inode(r, r->journal_inum, inode) || !(get_le32(inode + 0x20) & EXT4_EXTENTS_FL)) {
        rc = -LINUX_EINVAL;
        goto out;
    }
    uint64_t journal_bytes = (uint64_t)get_le32(inode + 0x04) | ((uint64_t)get_le32(inode + 0x6C) << 32);
    uint64_t journal_blocks = journal_bytes / r->block_size;
    if (journal_blocks < 2 || journal_blocks > UINT32_MAX) {
        rc = -LINUX_EINVAL;
        goto out;
    }
    s.log_map = calloc((size_t)journal_blocks, sizeof(*s.log_map));
    if (!s.log_map) {
        rc = -LINUX_ENOMEM;
        goto out;
    }
    if (!journal_map_extents(r, inode + 0x28, 60, s.log_map, (uint32_t)journal_blocks, EXT4_EXTENT_MAX_DEPTH) ||
        s.log_map[0] == 0 || !ext4_pread(r, jsb, r->block_size, s.log_map[0] * r->block_size)) {
        rc = -LINUX_EIO;
        goto out;
    }

    uint32_t type = get_be32(jsb + 4);
    if (get_be32(jsb) != JBD2_MAGIC || (type != JBD2_SUPERBLOCK_V1 && type != JBD2_SUPERBLOCK_V2) ||
        get_be32(jsb + 0x0C) != r->block_size) {
        rc = -LINUX_EINVAL;
        goto out;
    }
    uint32_t max_len = get_be32(jsb + 0x10);
    uint32_t first = get_be32(jsb + 0x14);
    uint32_t sequence = get_be32(jsb + 0x18);
    uint32_t start = get_be32(jsb + 0x1C);
    if (start == 0) {
        goto out;
    }
    s.incompat = type == JBD2_SUPERBLOCK_V2 ? get_be32(jsb + 0x28) : 0;
    const uint32_t known = JBD2_FEATURE_INCOMPAT_REVOKE | JBD2_FEATURE_INCOMPAT_64BIT |
                           JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT | JBD2_FEATURE_INCOMPAT_CSUM_V2 |
                           JBD2_FEATURE_INCOMPAT_CSUM_V3;
    if ((s.incompat & ~known) != 0 || max_len > journal_blocks || first == 0 || first >= max_len ||
        start < first || start >= max_len) {
        rc = -LINUX_EINVAL;
        goto out;
    }
    for (uint32_t i = 0; i < max_len; i++) {
        if (s.log_map[i] == 0) {
            rc = -LINUX_EINVAL;
            goto out;
        }
    }
    s.log_first = first;
    s.log_last = max_len;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1U) ? (c >> 1) ^ CRC32C_POLY : c >> 1;
        }
        s.crc_table[i] = c;
    }
    s.csum_seed = crc32c(s.crc_table, ~0U, jsb + 0x30, JBD2_UUID_SIZE);

    rc = journal_scan_log(&s, sequence, start);
    if (rc == 0) {
        rc = journal_build_replay(r, &s);
    }

out:
    free(s.tags);
    free(s.revokes);
    free(s.log_map);
    free(jsb);
    free(inode);
    return rc;
}


static uint64_t ext4_inode_size(const uint8_t *inode) {
    return (uint64_t)get_le32(inode + 0x04) | ((uint64_t)get_le32(inode + 0x6C) << 32);
}

typedef struct upper_stream {
    const ext4_reader_t *r;
    uint64_t size;
    uint64_t pos;
    ext4_data_cb cb;
    void *ctx;
} upper_stream_t;

static size_t stream_chunk(const upper_stream_t *s, uint64_t end) {
    return end - s->pos < UPPER_STREAM_CHUNK ? (size_t)(end - s->pos) : UPPER_STREAM_CHUNK;
}

static int stream_zeros(upper_stream_t *s, uint64_t end) {
    if (end > s->size) {
        end = s->size;
    }
    if (s->pos >= end) {
        return 0;
    }
    memset(s->r->stream_buf, 0, stream_chunk(s, end));
    while (s->pos < end) {
        size_t n = stream_chunk(s, end);
        int rc = s->cb(s->ctx, s->r->stream_buf, n);
        if (rc < 0) {
            return rc;
        }
        s->pos += n;
    }
    return 0;
}

static int stream_blocks(upper_stream_t *s, uint64_t logical, uint64_t phys, uint64_t count, bool unwritten) {
    const ext4_reader_t *r = s->r;
    uint64_t start = logical * r->block_size;
    if (start >= s->size) {
        return 0;
    }
    if (start < s->pos) {
        return -LINUX_EIO;
    }
    int rc = stream_zeros(s, start);
    if (rc < 0) {
        return rc;
    }
    uint64_t end = start + count * r->block_size;
    if (end > s->size) {
        end = s->size;
    }
    if (unwritten) {
        return stream_zeros(s, end);
    }
    if (phys >= r->total_blocks || count > r->total_blocks - phys) {
        return -LINUX_EIO;
    }
    uint64_t src = phys * r->block_size;
    while (s->pos < end) {
        size_t n = stream_chunk(s, end);
        if (!ext4_pread(r, r->stream_buf, n, src + (s->pos - start))) {
            return -LINUX_EIO;
        }
        rc = s->cb(s->ctx, r->stream_buf, n);
        if (rc < 0) {
            return rc;
        }
        s->pos += n;
    }
    return 0;
}

static int stream_extent_node(upper_stream_t *s, const uint8_t *node, size_t node_bytes, int expected_depth) {
    if (node_bytes < 12 || get_le16(node) != EXT4_EXTENT_MAGIC) {
        return -LINUX_EIO;
    }
    uint16_t entries = get_le16(node + 2);
    uint16_t depth = get_le16(node + 6);
    if (12 + (size_t)entries * 12 > node_bytes || depth > EXT4_EXTENT_MAX_DEPTH ||
        (expected_depth >= 0 && depth != expected_depth)) {
        return -LINUX_EIO;
    }
    if (depth == 0) {
        for (uint16_t i = 0; i < entries && s->pos < s->size; i++) {
            const uint8_t *e = node + 12 + (size_t)i * 12;
            uint16_t raw_len = get_le16(e + 4);
            bool unwritten = raw_len > EXT4_EXTENT_INIT_MAX_LEN;
            uint64_t phys = (uint64_t)get_le32(e + 8) | ((uint64_t)get_le16(e + 6) << 32);
            int rc = stream_blocks(s, get_le32(e + 0), phys,
                                   unwritten ? raw_len - EXT4_EXTENT_INIT_MAX_LEN : raw_len, unwritten);
            if (rc < 0) {
                return rc;
            }
        }
        return 0;
    }
    uint8_t *child = malloc(s->r->block_size);
    if (!child) {
        return -LINUX_ENOMEM;
    }
    int rc = 0;
    for (uint16_t i = 0; i < entries && rc == 0 && s->pos < s->size; i++) {
        const uint8_t *idx = node + 12 + (size_t)i * 12;
        uint64_t leaf = (uint64_t)get_le32(idx + 4) | ((uint64_t)get_le16(idx + 8) << 32);
        if (leaf >= s->r->total_blocks || !ext4_pread(s->r, child, s->r->block_size, leaf * s->r->block_size)) {
            rc = -LINUX_EIO;
        } else {
            rc = stream_extent_node(s, child, s->r->block_size, depth - 1);
        }
    }
    free(child);
    return rc;
}

static int stream_indirect(upper_stream_t *s, uint32_t block, unsigned level, uint64_t logical) {
    const ext4_reader_t *r = s->r;
    uint32_t per_block = r->block_size / 4;
    if (block >= r->total_blocks) {
        return -LINUX_EIO;
    }
    uint8_t *pointers = malloc(r->block_size);
    if (!pointers) {
        return -LINUX_ENOMEM;
    }
    if (!ext4_pread(r, pointers, r->block_size, (uint64_t)block * r->block_size)) {
        free(pointers);
        return -LINUX_EIO;
    }
    uint64_t span = 1;
    for (unsigned l = 1; l < level; l++) {
        span *= per_block;
    }
    int rc = 0;
    for (uint32_t i = 0; i < per_block && rc == 0; i++) {
        uint64_t child_logical = logical + (uint64_t)i * span;
        if (child_logical * r->block_size >= s->size) {
            break;
        }
        uint32_t child = get_le32(pointers + (size_t)i * 4);
        if (child == 0) {
            continue;
        }
        rc = level == 1 ? stream_blocks(s, child_logical, child, 1, false)
                        : stream_indirect(s, child, level - 1, child_logical);
    }
    free(pointers);
    return rc;
}

static int stream_block_map(upper_stream_t *s, const uint8_t *inode) {
    const ext4_reader_t *r = s->r;
    int rc = 0;
    for (uint32_t i = 0; i < EXT4_N_DIRECT_BLOCKS && rc == 0; i++) {
        uint32_t block = get_le32(inode + 0x28 + (size_t)i * 4);
        if ((uint64_t)i * r->block_size >= s->size) {
            return 0;
        }
        if (block != 0) {
            rc = stream_blocks(s, i, block, 1, false);
        }
    }
    uint64_t logical = EXT4_N_DIRECT_BLOCKS;
    uint64_t span = r->block_size / 4;
    for (unsigned level = 1; level <= 3 && rc == 0; level++) {
        if (logical * r->block_size >= s->size) {
            break;
        }
        uint32_t block = get_le32(inode + 0x28 + (size_t)(EXT4_N_DIRECT_BLOCKS + level - 1) * 4);
        if (block != 0) {
            rc = stream_indirect(s, block, level, logical);
        }
        logical += span;
        span *= r->block_size / 4;
    }
    return rc;
}

static int ext4_stream_inode(const ext4_reader_t *r, const uint8_t *inode, uint64_t size,
                             ext4_data_cb cb, void *ctx) {
    uint32_t flags = get_le32(inode + 0x20);
    if (flags & EXT4_INLINE_DATA_FL) {
        return -LINUX_EIO;
    }
    upper_stream_t s = {.r = r, .size = size, .cb = cb, .ctx = ctx};
    int rc = (flags & EXT4_EXTENTS_FL) ? stream_extent_node(&s, inode + 0x28, 60, -1) : stream_block_map(&s, inode);
    return rc < 0 ? rc : stream_zeros(&s, size);
}

typedef struct byte_buffer {
    uint8_t *data;
    size_t len;
    size_t cap;
} byte_buffer_t;

static int byte_buffer_collect(void *ctx, const void *data, size_t len) {
    byte_buffer_t *b = ctx;
    if (len > b->cap - b->len) {
        return -LINUX_EIO;
    }
    memcpy(b->data + b->len, data, len);
    b->len += len;
    return 0;
}

static int ext4_read_small_file(const ext4_reader_t *r, uint32_t ino, uint64_t expected_size, uint8_t **data_out) {
    *data_out = NULL;
    uint8_t *inode = malloc(r->inode_size);
    if (!inode) {
        return -LINUX_ENOMEM;
    }
    int rc = 0;
    byte_buffer_t b = {0};
    if (!ext4_read_inode(r, ino, inode) || ext4_inode_size(inode) != expected_size) {
        rc = -LINUX_EIO;
    } else {
        b.cap = (size_t)expected_size;
        b.data = malloc(b.cap > 0 ? b.cap : 1);
        rc = b.data ? ext4_stream_inode(r, inode, expected_size, byte_buffer_collect, &b) : -LINUX_ENOMEM;
    }
    free(inode);
    if (rc < 0) {
        free(b.data);
        return rc;
    }
    *data_out = b.data;
    return 0;
}

typedef struct xattr_list {
    ext4_xattr_info_t *items;
    size_t count;
    size_t cap;
    bool opaque;
} xattr_list_t;

static void xattr_list_free(xattr_list_t *l) {
    for (size_t i = 0; i < l->count; i++) {
        free((void *)(uintptr_t)l->items[i].name);
    }
    free(l->items);
    *l = (xattr_list_t){0};
}

static int xattr_list_add(xattr_list_t *l, const char *prefix, const uint8_t *suffix, size_t suffix_len,
                          const uint8_t *value, size_t size) {
    if (size > EXT4_XATTR_VALUE_MAX) {
        return -LINUX_EIO;
    }
    if (l->count == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 4;
        ext4_xattr_info_t *items = realloc(l->items, cap * sizeof(*items));
        if (!items) {
            return -LINUX_ENOMEM;
        }
        l->items = items;
        l->cap = cap;
    }
    size_t prefix_len = strlen(prefix);
    char *blob = malloc(prefix_len + suffix_len + 1 + size);
    if (!blob) {
        return -LINUX_ENOMEM;
    }
    memcpy(blob, prefix, prefix_len);
    if (suffix_len > 0) {
        memcpy(blob + prefix_len, suffix, suffix_len);
    }
    blob[prefix_len + suffix_len] = '\0';
    if (size > 0) {
        memcpy(blob + prefix_len + suffix_len + 1, value, size);
    }
    l->items[l->count++] = (ext4_xattr_info_t){
        .name = blob,
        .value = blob + prefix_len + suffix_len + 1,
        .size = size,
    };
    return 0;
}

static int xattr_list_add_acl(xattr_list_t *l, const char *name, const uint8_t *value, size_t size) {
    if (size < 4 || get_le32(value) != EXT4_ACL_VERSION) {
        return -LINUX_EIO;
    }
    size_t max_entries = (size - 4) / 4;
    uint8_t *posix = malloc(4 + max_entries * 8);
    if (!posix) {
        return -LINUX_ENOMEM;
    }
    put_le32(posix, POSIX_ACL_XATTR_VERSION);
    size_t out = 4;
    size_t off = 4;
    int rc = 0;
    while (off < size) {
        if (off + 4 > size) {
            rc = -LINUX_EIO;
            break;
        }
        uint16_t tag = get_le16(value + off);
        uint32_t id = EXT4_ACL_UNDEFINED_ID;
        size_t entry_size = 4;
        if (tag == EXT4_ACL_USER || tag == EXT4_ACL_GROUP) {
            if (off + 8 > size) {
                rc = -LINUX_EIO;
                break;
            }
            id = get_le32(value + off + 4);
            entry_size = 8;
        }
        memcpy(posix + out, value + off, 4);
        put_le32(posix + out + 4, id);
        out += 8;
        off += entry_size;
    }
    if (rc == 0) {
        rc = xattr_list_add(l, name, NULL, 0, posix, out);
    }
    free(posix);
    return rc;
}

static bool is_overlay_xattr(uint8_t index, const uint8_t *name, size_t name_len) {
    return (index == EXT4_XATTR_INDEX_TRUSTED || index == EXT4_XATTR_INDEX_USER) &&
           name_len >= 8 && memcmp(name, "overlay.", 8) == 0;
}

static int ext4_parse_xattrs(const ext4_reader_t *r, const uint8_t *values, const uint8_t *entries,
                             const uint8_t *end, xattr_list_t *l) {
    const uint8_t *p = entries;
    while (p + 4 <= end && get_le32(p) != 0) {
        if (p + EXT4_XATTR_ENTRY_SIZE > end) {
            return -LINUX_EIO;
        }
        uint8_t name_len = p[0];
        uint8_t index = p[1];
        uint16_t value_offs = get_le16(p + 2);
        uint32_t value_inum = get_le32(p + 4);
        uint32_t value_size = get_le32(p + 8);
        size_t entry_len = (EXT4_XATTR_ENTRY_SIZE + (size_t)name_len + 3U) & ~(size_t)3U;
        if (p + entry_len > end) {
            return -LINUX_EIO;
        }
        const uint8_t *name = p + EXT4_XATTR_ENTRY_SIZE;
        p += entry_len;

        const char *prefix = NULL;
        switch (index) {
        case EXT4_XATTR_INDEX_USER:
            prefix = "user.";
            break;
        case EXT4_XATTR_INDEX_TRUSTED:
            prefix = "trusted.";
            break;
        case EXT4_XATTR_INDEX_SECURITY:
            prefix = "security.";
            break;
        case EXT4_XATTR_INDEX_POSIX_ACL_ACCESS:
            prefix = "system.posix_acl_access";
            break;
        case EXT4_XATTR_INDEX_POSIX_ACL_DEFAULT:
            prefix = "system.posix_acl_default";
            break;
        default:
            continue;
        }

        uint8_t *owned = NULL;
        const uint8_t *value = NULL;
        if (value_inum != 0) {
            if (value_size > EXT4_XATTR_VALUE_MAX) {
                return -LINUX_EIO;
            }
            int rc = ext4_read_small_file(r, value_inum, value_size, &owned);
            if (rc < 0) {
                return rc;
            }
            value = owned;
        } else {
            if ((size_t)value_offs + value_size > (size_t)(end - values)) {
                return -LINUX_EIO;
            }
            value = values + value_offs;
        }

        int rc = 0;
        if (is_overlay_xattr(index, name, name_len)) {
            if (name_len == 14 && memcmp(name, "overlay.opaque", 14) == 0 && value_size >= 1 && value[0] == 'y') {
                l->opaque = true;
            }
        } else if (index == EXT4_XATTR_INDEX_POSIX_ACL_ACCESS || index == EXT4_XATTR_INDEX_POSIX_ACL_DEFAULT) {
            rc = name_len == 0 ? xattr_list_add_acl(l, prefix, value, value_size) : 0;
        } else {
            rc = xattr_list_add(l, prefix, name, name_len, value, value_size);
        }
        free(owned);
        if (rc < 0) {
            return rc;
        }
    }
    return 0;
}

static int ext4_read_xattrs(const ext4_reader_t *r, const uint8_t *inode, xattr_list_t *l) {
    if (r->inode_size > EXT4_GOOD_OLD_INODE_SIZE) {
        size_t ibody = EXT4_GOOD_OLD_INODE_SIZE + get_le16(inode + 0x80);
        if (ibody + 4 <= r->inode_size && get_le32(inode + ibody) == EXT4_XATTR_MAGIC) {
            int rc = ext4_parse_xattrs(r, inode + ibody + 4, inode + ibody + 4, inode + r->inode_size, l);
            if (rc < 0) {
                return rc;
            }
        }
    }
    uint64_t block = (uint64_t)get_le32(inode + 0x68) | ((uint64_t)get_le16(inode + 0x76) << 32);
    if (block == 0) {
        return 0;
    }
    if (block >= r->total_blocks) {
        return -LINUX_EIO;
    }
    uint8_t *buf = malloc(r->block_size);
    if (!buf) {
        return -LINUX_ENOMEM;
    }
    int rc = 0;
    if (!ext4_pread(r, buf, r->block_size, block * r->block_size) ||
        get_le32(buf) != EXT4_XATTR_MAGIC || get_le32(buf + 8) != 1) {
        rc = -LINUX_EIO;
    } else {
        rc = ext4_parse_xattrs(r, buf, buf + EXT4_XATTR_BLOCK_HEADER_SIZE, buf + r->block_size, l);
    }
    free(buf);
    return rc;
}

typedef struct dir_listing {
    struct dir_item {
        uint32_t ino;
        uint32_t name_off;
        uint8_t name_len;
    } *items;
    size_t count;
    size_t cap;
    char *names;
    size_t names_len;
    size_t names_cap;
    uint32_t block_size;
} dir_listing_t;

static void dir_listing_free(dir_listing_t *d) {
    free(d->items);
    free(d->names);
    *d = (dir_listing_t){0};
}

static int dir_listing_add(dir_listing_t *d, uint32_t ino, const uint8_t *name, uint8_t name_len) {
    if (d->count == d->cap) {
        size_t cap = d->cap ? d->cap * 2 : 32;
        struct dir_item *items = realloc(d->items, cap * sizeof(*items));
        if (!items) {
            return -LINUX_ENOMEM;
        }
        d->items = items;
        d->cap = cap;
    }
    if (name_len > d->names_cap - d->names_len) {
        size_t cap = d->names_cap ? d->names_cap * 2 : 1024;
        while (cap - d->names_len < name_len) {
            cap *= 2;
        }
        char *names = realloc(d->names, cap);
        if (!names) {
            return -LINUX_ENOMEM;
        }
        d->names = names;
        d->names_cap = cap;
    }
    if (d->names_len > UINT32_MAX - name_len) {
        return -LINUX_ENOMEM;
    }
    memcpy(d->names + d->names_len, name, name_len);
    d->items[d->count++] = (struct dir_item){
        .ino = ino,
        .name_off = (uint32_t)d->names_len,
        .name_len = name_len,
    };
    d->names_len += name_len;
    return 0;
}

static int dir_listing_collect(void *ctx, const void *data, size_t len) {
    dir_listing_t *d = ctx;
    const uint8_t *chunk = data;
    for (size_t block = 0; block < len; block += d->block_size) {
        size_t block_len = len - block < d->block_size ? len - block : d->block_size;
        const uint8_t *p = chunk + block;
        size_t off = 0;
        while (off + 8 <= block_len) {
            uint32_t ino = get_le32(p + off);
            size_t rec_len = get_le16(p + off + 4);
            uint8_t name_len = p[off + 6];
            if (rec_len == 0 && d->block_size == 65536) {
                rec_len = d->block_size;
            }
            if (rec_len < 8 || off + rec_len > block_len || (size_t)name_len + 8 > rec_len) {
                return -LINUX_EIO;
            }
            const uint8_t *name = p + off + 8;
            bool dot = (name_len == 1 && name[0] == '.') || (name_len == 2 && name[0] == '.' && name[1] == '.');
            if (ino != 0 && name_len != 0 && !dot) {
                int rc = dir_listing_add(d, ino, name, name_len);
                if (rc < 0) {
                    return rc;
                }
            }
            off += rec_len;
        }
    }
    return 0;
}

static int ext4_list_dir(const ext4_reader_t *r, uint32_t ino, dir_listing_t *out) {
    *out = (dir_listing_t){.block_size = r->block_size};
    uint8_t *inode = malloc(r->inode_size);
    if (!inode) {
        return -LINUX_ENOMEM;
    }
    int rc = 0;
    if (!ext4_read_inode(r, ino, inode) || !S_ISDIR(get_le16(inode))) {
        rc = -LINUX_EIO;
    } else {
        rc = ext4_stream_inode(r, inode, ext4_inode_size(inode), dir_listing_collect, out);
    }
    free(inode);
    if (rc < 0) {
        dir_listing_free(out);
    }
    return rc;
}

static int ext4_lookup_child(const ext4_reader_t *r, uint32_t dir_ino, const char *target, uint32_t *ino_out) {
    *ino_out = 0;
    dir_listing_t d;
    int rc = ext4_list_dir(r, dir_ino, &d);
    if (rc < 0) {
        return rc;
    }
    size_t target_len = strlen(target);
    for (size_t i = 0; i < d.count; i++) {
        if (d.items[i].name_len == target_len && memcmp(d.names + d.items[i].name_off, target, target_len) == 0) {
            *ino_out = d.items[i].ino;
            break;
        }
    }
    dir_listing_free(&d);
    return 0;
}

static uint64_t ext4_decode_rdev(const uint8_t *ib) {
    uint32_t old_dev = get_le32(ib + 0);
    if (old_dev != 0) {
        uint32_t maj = (old_dev >> 8) & 0xFFU;
        uint32_t min = old_dev & 0xFFU;
        return ((uint64_t)maj << 32) | min;
    }
    uint32_t new_dev = get_le32(ib + 4);
    uint32_t maj = (new_dev >> 8) & 0xFFFU;
    uint32_t min = (new_dev & 0xFFU) | ((new_dev >> 12) & 0xFFF00U);
    return ((uint64_t)maj << 32) | min;
}

typedef struct ino_set {
    uint32_t *slots;
    size_t cap;
    size_t count;
} ino_set_t;

static int ino_set_insert(ino_set_t *s, uint32_t ino) {
    if ((s->count + 1) * 2 > s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 256;
        uint32_t *slots = calloc(cap, sizeof(*slots));
        if (!slots) {
            return -LINUX_ENOMEM;
        }
        for (size_t i = 0; i < s->cap; i++) {
            if (s->slots[i] != 0) {
                size_t pos = (s->slots[i] * 2654435761U) & (cap - 1);
                while (slots[pos] != 0) {
                    pos = (pos + 1) & (cap - 1);
                }
                slots[pos] = s->slots[i];
            }
        }
        free(s->slots);
        s->slots = slots;
        s->cap = cap;
    }
    size_t pos = (ino * 2654435761U) & (s->cap - 1);
    while (s->slots[pos] != 0) {
        if (s->slots[pos] == ino) {
            return 0;
        }
        pos = (pos + 1) & (s->cap - 1);
    }
    s->slots[pos] = ino;
    s->count++;
    return 1;
}

struct swipterm_upper {
    ext4_reader_t reader;
    uint32_t root_ino;
};

int swipterm_upper_open(const char *path, swipterm_upper_t **upper_out) {
    *upper_out = NULL;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return errno == ENOENT ? 0 : -LINUX_EIO;
    }
    swipterm_upper_t *upper = calloc(1, sizeof(*upper));
    if (!upper) {
        close(fd);
        return -LINUX_ENOMEM;
    }
    ext4_reader_t *r = &upper->reader;
    r->fd = fd;
    bool blank = false;
    int rc = ext4_load(r, &blank);
    if (rc == 0 && !blank && (r->incompat & EXT4_FEATURE_INCOMPAT_RECOVER)) {
        rc = journal_replay(r);
        if (rc == 0 && r->replay_count > 0) {
            rc = ext4_load(r, &blank);
        }
    }
    if (rc == 0 && !blank) {
        r->stream_buf = malloc(UPPER_STREAM_CHUNK);
        rc = r->stream_buf ? ext4_lookup_child(r, EXT4_ROOT_INO, "root", &upper->root_ino) : -LINUX_ENOMEM;
        if (rc == 0 && upper->root_ino == 0) {
            rc = ext4_lookup_child(r, EXT4_ROOT_INO, "upper", &upper->root_ino);
        }
    }
    if (rc < 0 || blank || upper->root_ino == 0) {
        swipterm_upper_close(upper);
        return rc;
    }
    *upper_out = upper;
    return 0;
}

void swipterm_upper_close(swipterm_upper_t *upper) {
    if (!upper) {
        return;
    }
    free(upper->reader.stream_buf);
    free(upper->reader.replay);
    free(upper->reader.gdt);
    close(upper->reader.fd);
    free(upper);
}

int swipterm_upper_read(swipterm_upper_t *upper, uint32_t ino, ext4_data_cb cb, void *ctx) {
    const ext4_reader_t *r = &upper->reader;
    uint8_t *inode = malloc(r->inode_size);
    if (!inode) {
        return -LINUX_ENOMEM;
    }
    int rc = 0;
    if (!ext4_read_inode(r, ino, inode) || !S_ISREG(get_le16(inode))) {
        rc = -LINUX_EIO;
    } else {
        rc = ext4_stream_inode(r, inode, ext4_inode_size(inode), cb, ctx);
    }
    free(inode);
    return rc;
}

static int ext4_visit_entry(const ext4_reader_t *r, uint32_t ino, const char *name, size_t name_len,
                            uint8_t *inode, void *parent, ext4_walk_cb cb, void *ctx, uint32_t *mode_out,
                            void **child_out) {
    *child_out = NULL;
    if (!ext4_read_inode(r, ino, inode)) {
        return -LINUX_EIO;
    }
    uint32_t mode = get_le16(inode + 0x00);
    uint64_t size = ext4_inode_size(inode);
    int64_t mtime = (int32_t)get_le32(inode + 0x10);
    uint32_t mtime_nsec = 0;
    if (r->inode_size >= EXT4_GOOD_OLD_INODE_SIZE + 12 &&
        EXT4_GOOD_OLD_INODE_SIZE + (size_t)get_le16(inode + 0x80) >= 0x8C) {
        uint32_t extra = get_le32(inode + 0x88);
        mtime += (int64_t)(extra & 3U) << 32;
        mtime_nsec = extra >> 2;
    }
    ext4_entry_info_t info = {
        .name = name,
        .name_len = name_len,
        .ino = ino,
        .mode = mode,
        .uid = get_le16(inode + 0x02) | ((uint32_t)get_le16(inode + 0x78) << 16),
        .gid = get_le16(inode + 0x18) | ((uint32_t)get_le16(inode + 0x7A) << 16),
        .nlink = get_le16(inode + 0x1A),
        .size = size,
        .mtime = mtime,
        .mtime_nsec = mtime_nsec,
    };

    xattr_list_t xattrs = {0};
    int rc = ext4_read_xattrs(r, inode, &xattrs);
    char *symlink_target = NULL;
    if (rc == 0 && S_ISLNK(mode)) {
        if (size >= EXT4_SYMLINK_MAX) {
            rc = -LINUX_EIO;
        } else if (size < EXT4_FAST_SYMLINK_MAX && !(get_le32(inode + 0x20) & EXT4_INLINE_DATA_FL)) {
            symlink_target = malloc((size_t)size + 1);
            if (symlink_target) {
                memcpy(symlink_target, inode + 0x28, (size_t)size);
            } else {
                rc = -LINUX_ENOMEM;
            }
        } else {
            uint8_t *raw = NULL;
            rc = ext4_read_small_file(r, ino, size, &raw);
            if (rc == 0) {
                symlink_target = realloc(raw, (size_t)size + 1);
                if (!symlink_target) {
                    free(raw);
                    rc = -LINUX_ENOMEM;
                }
            }
        }
        if (symlink_target) {
            symlink_target[size] = '\0';
            info.symlink_target = symlink_target;
        }
    } else if (S_ISCHR(mode) || S_ISBLK(mode)) {
        info.rdev = ext4_decode_rdev(inode + 0x28);
    }
    if (rc == 0) {
        info.opaque_dir = S_ISDIR(mode) && xattrs.opaque;
        info.xattrs = xattrs.items;
        info.xattr_count = xattrs.count;
        rc = cb(ctx, parent, &info, child_out);
    }
    free(symlink_target);
    xattr_list_free(&xattrs);
    *mode_out = mode;
    return rc;
}

int swipterm_upper_walk(swipterm_upper_t *upper, void *root, ext4_walk_cb cb, void *ctx) {
    const ext4_reader_t *r = &upper->reader;
    typedef struct walk_frame {
        dir_listing_t listing;
        size_t next;
        void *parent;
    } walk_frame_t;

    uint8_t *inode = malloc(r->inode_size);
    walk_frame_t *frames = malloc(16 * sizeof(*frames));
    if (!inode || !frames) {
        free(inode);
        free(frames);
        return -LINUX_ENOMEM;
    }
    size_t depth = 0;
    size_t frames_cap = 16;
    ino_set_t visited = {0};
    int rc = ino_set_insert(&visited, upper->root_ino);
    if (rc >= 0) {
        rc = ext4_list_dir(r, upper->root_ino, &frames[0].listing);
    }
    if (rc >= 0) {
        frames[0].next = 0;
        frames[0].parent = root;
        depth = 1;
        rc = 0;
    }
    while (rc == 0 && depth > 0) {
        walk_frame_t *frame = &frames[depth - 1];
        if (frame->next == frame->listing.count) {
            dir_listing_free(&frame->listing);
            depth--;
            continue;
        }
        const struct dir_item *item = &frame->listing.items[frame->next++];
        uint32_t mode = 0;
        void *child = NULL;
        rc = ext4_visit_entry(r, item->ino, frame->listing.names + item->name_off, item->name_len,
                              inode, frame->parent, cb, ctx, &mode, &child);
        if (rc < 0 || !S_ISDIR(mode) || !child) {
            continue;
        }
        int inserted = ino_set_insert(&visited, item->ino);
        if (inserted <= 0) {
            rc = inserted < 0 ? inserted : -LINUX_EIO;
            continue;
        }
        if (depth == frames_cap) {
            walk_frame_t *grown = realloc(frames, frames_cap * 2 * sizeof(*frames));
            if (!grown) {
                rc = -LINUX_ENOMEM;
                continue;
            }
            frames = grown;
            frames_cap *= 2;
        }
        rc = ext4_list_dir(r, item->ino, &frames[depth].listing);
        if (rc == 0) {
            frames[depth].next = 0;
            frames[depth].parent = child;
            depth++;
        }
    }
    while (depth > 0) {
        dir_listing_free(&frames[--depth].listing);
    }
    free(visited.slots);
    free(frames);
    free(inode);
    return rc;
}
