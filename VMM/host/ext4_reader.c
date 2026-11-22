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
    EXT4_FEATURE_COMPAT_SPARSE_SUPER2 = 0x0200,
    EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER = 0x0001,
    EXT4_FEATURE_INCOMPAT_FILETYPE = 0x0002,
    EXT4_FEATURE_INCOMPAT_RECOVER = 0x0004,
    EXT4_FEATURE_INCOMPAT_META_BG = 0x0010,
    EXT4_FEATURE_INCOMPAT_EXTENTS = 0x0040,
    EXT4_FEATURE_INCOMPAT_64BIT = 0x0080,
    EXT4_FEATURE_INCOMPAT_MMP = 0x0100,
    EXT4_FEATURE_INCOMPAT_FLEX_BG = 0x0200,
    EXT4_FEATURE_INCOMPAT_EA_INODE = 0x0400,
    EXT4_FEATURE_INCOMPAT_CSUM_SEED = 0x2000,
    EXT4_FEATURE_INCOMPAT_LARGEDIR = 0x4000,

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
    EXT4_XATTR_VALUE_MAX = 65536,
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
    STREAM_CHUNK = 1024 * 1024,
    RESOLVE_MAX_SYMLINKS = 40,
    RESOLVE_MAX_DEPTH = 256,

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
    JBD2_FEATURE_INCOMPAT_FAST_COMMIT = 0x20,
    JBD2_DEFAULT_FAST_COMMIT_BLOCKS = 256,
    JBD2_FLAG_ESCAPE = 0x01,
    JBD2_FLAG_SAME_UUID = 0x02,
    JBD2_FLAG_LAST_TAG = 0x08,
    JBD2_HEADER_SIZE = 12,
    JBD2_REVOKE_HEADER_SIZE = 16,
    JBD2_COMMIT_CHECKSUM_OFFSET = 16,
    JBD2_UUID_SIZE = 16,

    CRC32C_POLY = 0x82F63B78,
};

static const uint32_t SUPPORTED_INCOMPAT =
    EXT4_FEATURE_INCOMPAT_FILETYPE | EXT4_FEATURE_INCOMPAT_RECOVER | EXT4_FEATURE_INCOMPAT_META_BG |
    EXT4_FEATURE_INCOMPAT_EXTENTS | EXT4_FEATURE_INCOMPAT_64BIT | EXT4_FEATURE_INCOMPAT_MMP |
    EXT4_FEATURE_INCOMPAT_FLEX_BG | EXT4_FEATURE_INCOMPAT_EA_INODE | EXT4_FEATURE_INCOMPAT_CSUM_SEED |
    EXT4_FEATURE_INCOMPAT_LARGEDIR;

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
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
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

static int pread_all(int fd, void *buf, size_t len, off_t off) {
    uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = pread(fd, p, len, off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -errno;
        }
        if (n == 0) {
            memset(p, 0, len);
            return 0;
        }
        p += (size_t)n;
        len -= (size_t)n;
        off += (off_t)n;
    }
    return 0;
}

typedef struct replay_block {
    uint64_t fs_block;
    uint64_t log_block;
    bool escaped;
} replay_block_t;

struct ext4_fs {
    int fd;
    uint32_t block_size;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t inode_size;
    uint32_t desc_size;
    uint32_t num_groups;
    uint32_t first_data_block;
    uint32_t first_meta_bg;
    uint32_t compat;
    uint32_t incompat;
    uint32_t ro_compat;
    uint32_t backup_bgs[2];
    uint32_t journal_inum;
    uint64_t total_blocks;
    uint64_t inodes_count;
    uint64_t free_inodes;
    uint8_t *gdt;
    uint8_t *stream_buf;
    uint8_t *inode_buf;
    replay_block_t *replay;
    size_t replay_count;
};

static size_t replay_lower_bound(const ext4_fs_t *fs, uint64_t block) {
    size_t lo = 0;
    size_t hi = fs->replay_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (fs->replay[mid].fs_block < block) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

static int ext4_pread(const ext4_fs_t *fs, void *buf, size_t len, uint64_t off) {
    if (fs->replay_count == 0) {
        return pread_all(fs->fd, buf, len, (off_t)off);
    }
    uint8_t *p = buf;
    while (len > 0) {
        uint64_t block = off / fs->block_size;
        size_t idx = replay_lower_bound(fs, block);
        size_t chunk;
        int rc;
        if (idx == fs->replay_count || fs->replay[idx].fs_block != block) {
            uint64_t end = idx == fs->replay_count ? UINT64_MAX : fs->replay[idx].fs_block * fs->block_size;
            chunk = end - off < len ? (size_t)(end - off) : len;
            rc = pread_all(fs->fd, p, chunk, (off_t)off);
        } else {
            const replay_block_t *e = &fs->replay[idx];
            uint32_t in_block = (uint32_t)(off % fs->block_size);
            chunk = fs->block_size - in_block < len ? fs->block_size - in_block : len;
            rc = pread_all(fs->fd, p, chunk, (off_t)(e->log_block * fs->block_size + in_block));
            if (rc == 0 && e->escaped && in_block < 4) {
                uint8_t magic[4];
                put_be32(magic, JBD2_MAGIC);
                size_t n = 4 - in_block < chunk ? 4 - in_block : chunk;
                memcpy(p, magic + in_block, n);
            }
        }
        if (rc < 0) {
            return rc;
        }
        p += chunk;
        off += chunk;
        len -= chunk;
    }
    return 0;
}

static bool is_power_of(uint32_t n, uint32_t base) {
    while (n > 1 && n % base == 0) {
        n /= base;
    }
    return n == 1;
}

static bool group_has_super(const ext4_fs_t *fs, uint32_t group) {
    if (group == 0) {
        return true;
    }
    if (fs->compat & EXT4_FEATURE_COMPAT_SPARSE_SUPER2) {
        return group == fs->backup_bgs[0] || group == fs->backup_bgs[1];
    }
    if (!(fs->ro_compat & EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER) || group == 1) {
        return true;
    }
    if ((group & 1U) == 0) {
        return false;
    }
    return is_power_of(group, 3) || is_power_of(group, 5) || is_power_of(group, 7);
}

static uint64_t gdt_block_location(const ext4_fs_t *fs, uint32_t index) {
    if (!(fs->incompat & EXT4_FEATURE_INCOMPAT_META_BG) || index < fs->first_meta_bg) {
        return (uint64_t)fs->first_data_block + 1 + index;
    }
    uint32_t group = index * (fs->block_size / fs->desc_size);
    return (uint64_t)fs->first_data_block + (uint64_t)group * fs->blocks_per_group +
           (group_has_super(fs, group) ? 1U : 0U);
}

static int ext4_load(ext4_fs_t *fs) {
    uint8_t sb[1024];
    int rc = ext4_pread(fs, sb, sizeof(sb), EXT4_SUPER_OFFSET);
    if (rc < 0) {
        return rc;
    }
    if (get_le16(sb + 0x38) != EXT4_SUPER_MAGIC) {
        return -EINVAL;
    }
    uint32_t log_bs = get_le32(sb + 0x18);
    if (log_bs > 6) {
        return -EINVAL;
    }
    fs->block_size = 1024U << log_bs;
    fs->first_data_block = get_le32(sb + 0x14);
    fs->blocks_per_group = get_le32(sb + 0x20);
    fs->inodes_per_group = get_le32(sb + 0x28);
    fs->inode_size = get_le32(sb + 0x4C) == 0 ? EXT4_GOOD_OLD_INODE_SIZE : get_le16(sb + 0x58);
    fs->compat = get_le32(sb + 0x5C);
    fs->incompat = get_le32(sb + 0x60);
    fs->ro_compat = get_le32(sb + 0x64);
    fs->first_meta_bg = get_le32(sb + 0x104);
    fs->backup_bgs[0] = get_le32(sb + 0x24C);
    fs->backup_bgs[1] = get_le32(sb + 0x250);
    fs->desc_size = (fs->incompat & EXT4_FEATURE_INCOMPAT_64BIT) ? get_le16(sb + 0xFE) : 32U;
    fs->journal_inum = (fs->compat & EXT4_FEATURE_COMPAT_HAS_JOURNAL) ? get_le32(sb + 0xE0) : 0;
    fs->inodes_count = get_le32(sb + 0x00);
    fs->free_inodes = get_le32(sb + 0x10);
    if (fs->incompat & ~SUPPORTED_INCOMPAT) {
        return -ENOTSUP;
    }
    if (fs->inode_size < EXT4_GOOD_OLD_INODE_SIZE || fs->inode_size > fs->block_size ||
        fs->inodes_per_group == 0 || fs->blocks_per_group == 0 || fs->desc_size < 32 ||
        fs->desc_size > fs->block_size || (fs->desc_size & (fs->desc_size - 1)) != 0) {
        return -EINVAL;
    }
    fs->total_blocks = get_le32(sb + 0x04);
    if (fs->incompat & EXT4_FEATURE_INCOMPAT_64BIT) {
        fs->total_blocks |= (uint64_t)get_le32(sb + 0x150) << 32;
    }
    if (fs->total_blocks <= fs->first_data_block) {
        return -EINVAL;
    }
    uint64_t num_groups = (fs->total_blocks - fs->first_data_block + fs->blocks_per_group - 1) / fs->blocks_per_group;
    if (num_groups == 0 || num_groups > (1U << 24)) {
        return -EINVAL;
    }
    fs->num_groups = (uint32_t)num_groups;

    uint32_t per_block = fs->block_size / fs->desc_size;
    uint32_t gdt_blocks = (fs->num_groups + per_block - 1) / per_block;
    free(fs->gdt);
    fs->gdt = malloc((size_t)gdt_blocks * fs->block_size);
    if (!fs->gdt) {
        return -ENOMEM;
    }
    for (uint32_t i = 0; i < gdt_blocks; i++) {
        uint64_t block = gdt_block_location(fs, i);
        if (block >= fs->total_blocks) {
            return -EINVAL;
        }
        rc = ext4_pread(fs, fs->gdt + (size_t)i * fs->block_size, fs->block_size, block * fs->block_size);
        if (rc < 0) {
            return rc;
        }
    }
    return 0;
}

static int ext4_read_inode(const ext4_fs_t *fs, uint32_t ino, uint8_t *buf) {
    if (ino == 0) {
        return -EINVAL;
    }
    uint32_t idx = ino - 1;
    uint32_t g = idx / fs->inodes_per_group;
    uint32_t local = idx % fs->inodes_per_group;
    if (g >= fs->num_groups) {
        return -EINVAL;
    }
    const uint8_t *gd = fs->gdt + (size_t)g * fs->desc_size;
    uint64_t itable = get_le32(gd + 0x08);
    if (fs->desc_size >= 64) {
        itable |= (uint64_t)get_le32(gd + 0x28) << 32;
    }
    if (itable == 0 || itable >= fs->total_blocks) {
        return -EIO;
    }
    return ext4_pread(fs, buf, fs->inode_size, itable * fs->block_size + (uint64_t)local * fs->inode_size);
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
    const ext4_fs_t *fs;
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

static int journal_map_extents(const ext4_fs_t *fs, const uint8_t *node, size_t node_bytes,
                               uint64_t *map, uint32_t count, uint32_t depth_left) {
    if (node_bytes < 12 || get_le16(node) != EXT4_EXTENT_MAGIC) {
        return -EIO;
    }
    uint16_t entries = get_le16(node + 2);
    uint16_t depth = get_le16(node + 6);
    if (12 + (size_t)entries * 12 > node_bytes || depth > depth_left) {
        return -EIO;
    }
    if (depth == 0) {
        for (uint16_t i = 0; i < entries; i++) {
            const uint8_t *e = node + 12 + (size_t)i * 12;
            uint32_t logical = get_le32(e + 0);
            uint16_t raw_len = get_le16(e + 4);
            uint32_t len = raw_len > EXT4_EXTENT_INIT_MAX_LEN ? (uint32_t)(raw_len - EXT4_EXTENT_INIT_MAX_LEN) : raw_len;
            uint64_t phys = (uint64_t)get_le32(e + 8) | ((uint64_t)get_le16(e + 6) << 32);
            for (uint32_t k = 0; k < len && logical + k < count; k++) {
                map[logical + k] = phys + k;
            }
        }
        return 0;
    }
    uint8_t *child = malloc(fs->block_size);
    if (!child) {
        return -ENOMEM;
    }
    int rc = 0;
    for (uint16_t i = 0; i < entries && rc == 0; i++) {
        const uint8_t *idx = node + 12 + (size_t)i * 12;
        uint64_t leaf = (uint64_t)get_le32(idx + 4) | ((uint64_t)get_le16(idx + 8) << 32);
        rc = ext4_pread(fs, child, fs->block_size, leaf * fs->block_size);
        if (rc == 0) {
            rc = journal_map_extents(fs, child, fs->block_size, map, count, depth - 1);
        }
    }
    free(child);
    return rc;
}

static int journal_read(const journal_scan_t *s, uint32_t log_block, uint8_t *buf) {
    return ext4_pread(s->fs, buf, s->fs->block_size, s->log_map[log_block] * s->fs->block_size);
}

static uint32_t journal_next(const journal_scan_t *s, uint32_t log_block) {
    log_block++;
    return log_block >= s->log_last ? s->log_first : log_block;
}

static bool journal_tail_valid(const journal_scan_t *s, const uint8_t *buf) {
    static const uint8_t zero[4];
    uint32_t bs = s->fs->block_size;
    uint32_t crc = crc32c(s->crc_table, s->csum_seed, buf, bs - 4);
    crc = crc32c(s->crc_table, crc, zero, sizeof(zero));
    return get_be32(buf + bs - 4) == crc;
}

static bool journal_commit_valid(const journal_scan_t *s, const uint8_t *buf) {
    static const uint8_t zero[4];
    uint32_t bs = s->fs->block_size;
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
    uint32_t bs = s->fs->block_size;
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
        return -ENOMEM;
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
        rc = journal_read(s, block, buf);
        if (rc < 0) {
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
                bool keep = fs_block < s->fs->total_blocks;
                if (keep && csum) {
                    rc = journal_read(s, log_block, data);
                    if (rc < 0) {
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
                    rc = -ENOMEM;
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
                    rc = -ENOMEM;
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

static int journal_build_replay(ext4_fs_t *fs, journal_scan_t *s) {
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

    fs->replay = calloc(live > 0 ? live : 1, sizeof(*fs->replay));
    if (!fs->replay) {
        return -ENOMEM;
    }
    size_t count = 0;
    for (size_t i = 0; i < live; i++) {
        if (i + 1 < live && s->tags[i + 1].fs_block == s->tags[i].fs_block) {
            continue;
        }
        fs->replay[count++] = (replay_block_t){
            .fs_block = s->tags[i].fs_block,
            .log_block = s->tags[i].log_block,
            .escaped = s->tags[i].escaped,
        };
    }
    fs->replay_count = count;
    return 0;
}

static int journal_replay(ext4_fs_t *fs) {
    if (fs->journal_inum == 0) {
        return -EINVAL;
    }
    uint8_t *inode = malloc(fs->inode_size);
    uint8_t *jsb = malloc(fs->block_size);
    journal_scan_t s = {.fs = fs};
    int rc = 0;
    if (!inode || !jsb) {
        rc = -ENOMEM;
        goto out;
    }
    rc = ext4_read_inode(fs, fs->journal_inum, inode);
    if (rc < 0) {
        goto out;
    }
    if (!(get_le32(inode + 0x20) & EXT4_EXTENTS_FL)) {
        rc = -EINVAL;
        goto out;
    }
    uint64_t journal_bytes = (uint64_t)get_le32(inode + 0x04) | ((uint64_t)get_le32(inode + 0x6C) << 32);
    uint64_t journal_blocks = journal_bytes / fs->block_size;
    if (journal_blocks < 2 || journal_blocks > UINT32_MAX) {
        rc = -EINVAL;
        goto out;
    }
    s.log_map = calloc((size_t)journal_blocks, sizeof(*s.log_map));
    if (!s.log_map) {
        rc = -ENOMEM;
        goto out;
    }
    rc = journal_map_extents(fs, inode + 0x28, 60, s.log_map, (uint32_t)journal_blocks, EXT4_EXTENT_MAX_DEPTH);
    if (rc == 0 && s.log_map[0] == 0) {
        rc = -EIO;
    }
    if (rc == 0) {
        rc = ext4_pread(fs, jsb, fs->block_size, s.log_map[0] * fs->block_size);
    }
    if (rc < 0) {
        goto out;
    }

    uint32_t type = get_be32(jsb + 4);
    if (get_be32(jsb) != JBD2_MAGIC || (type != JBD2_SUPERBLOCK_V1 && type != JBD2_SUPERBLOCK_V2) ||
        get_be32(jsb + 0x0C) != fs->block_size) {
        rc = -EINVAL;
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
                           JBD2_FEATURE_INCOMPAT_CSUM_V3 | JBD2_FEATURE_INCOMPAT_FAST_COMMIT;
    uint32_t log_last = max_len;
    if (s.incompat & JBD2_FEATURE_INCOMPAT_FAST_COMMIT) {
        uint32_t fc_blocks = get_be32(jsb + 0x54);
        fc_blocks = fc_blocks == 0 ? JBD2_DEFAULT_FAST_COMMIT_BLOCKS : fc_blocks;
        log_last = fc_blocks < max_len ? max_len - fc_blocks : 0;
    }
    if ((s.incompat & ~known) != 0 || max_len > journal_blocks || first == 0 || first >= log_last ||
        start < first || start >= log_last) {
        rc = -EINVAL;
        goto out;
    }
    for (uint32_t i = 0; i < log_last; i++) {
        if (s.log_map[i] == 0) {
            rc = -EINVAL;
            goto out;
        }
    }
    s.log_first = first;
    s.log_last = log_last;
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
        rc = journal_build_replay(fs, &s);
    }

out:
    free(s.tags);
    free(s.revokes);
    free(s.log_map);
    free(jsb);
    free(inode);
    return rc;
}

int ext4_fs_open(int fd, ext4_fs_t **fs_out) {
    *fs_out = NULL;
    ext4_fs_t *fs = calloc(1, sizeof(*fs));
    if (!fs) {
        return -ENOMEM;
    }
    fs->fd = fd;
    int rc = ext4_load(fs);
    if (rc == 0 && (fs->incompat & EXT4_FEATURE_INCOMPAT_RECOVER)) {
        rc = journal_replay(fs);
        if (rc == 0 && fs->replay_count > 0) {
            rc = ext4_load(fs);
        }
    }
    if (rc == 0) {
        fs->stream_buf = malloc(STREAM_CHUNK);
        fs->inode_buf = malloc(fs->inode_size);
        if (!fs->stream_buf || !fs->inode_buf) {
            rc = -ENOMEM;
        }
    }
    if (rc < 0) {
        ext4_fs_close(fs);
        return rc;
    }
    *fs_out = fs;
    return 0;
}

void ext4_fs_close(ext4_fs_t *fs) {
    if (!fs) {
        return;
    }
    free(fs->stream_buf);
    free(fs->inode_buf);
    free(fs->replay);
    free(fs->gdt);
    free(fs);
}

uint64_t ext4_fs_used_inodes(const ext4_fs_t *fs) {
    return fs->inodes_count > fs->free_inodes ? fs->inodes_count - fs->free_inodes : 0;
}

static uint64_t inode_size_field(const uint8_t *inode) {
    return (uint64_t)get_le32(inode + 0x04) | ((uint64_t)get_le32(inode + 0x6C) << 32);
}

static bool inode_has_extra(const ext4_fs_t *fs, const uint8_t *inode, size_t field_end) {
    return fs->inode_size > EXT4_GOOD_OLD_INODE_SIZE &&
           EXT4_GOOD_OLD_INODE_SIZE + (size_t)get_le16(inode + 0x80) >= field_end;
}

static int64_t inode_time(const ext4_fs_t *fs, const uint8_t *inode, size_t base, size_t extra, uint32_t *nsec_out) {
    int64_t sec = (int32_t)get_le32(inode + base);
    uint32_t nsec = 0;
    if (inode_has_extra(fs, inode, extra + 4)) {
        uint32_t e = get_le32(inode + extra);
        sec += (int64_t)(e & 3U) << 32;
        nsec = e >> 2;
    }
    *nsec_out = nsec;
    return sec;
}

static void inode_to_node(const ext4_fs_t *fs, uint32_t ino, const uint8_t *inode, ext4_node_t *node) {
    uint32_t mnsec = 0;
    uint32_t cnsec = 0;
    int64_t mtime = inode_time(fs, inode, 0x10, 0x88, &mnsec);
    int64_t ctime = inode_time(fs, inode, 0x0C, 0x84, &cnsec);
    *node = (ext4_node_t){
        .ino = ino,
        .mode = get_le16(inode + 0x00),
        .uid = get_le16(inode + 0x02) | ((uint32_t)get_le16(inode + 0x78) << 16),
        .gid = get_le16(inode + 0x18) | ((uint32_t)get_le16(inode + 0x7A) << 16),
        .nlink = get_le16(inode + 0x1A),
        .generation = get_le32(inode + 0x64),
        .size = inode_size_field(inode),
        .mtime_ns = mtime * 1000000000LL + mnsec,
        .ctime_ns = ctime * 1000000000LL + cnsec,
    };
}

int ext4_fs_stat(ext4_fs_t *fs, uint32_t ino, ext4_node_t *node_out) {
    int rc = ext4_read_inode(fs, ino, fs->inode_buf);
    if (rc < 0) {
        return rc;
    }
    if (get_le16(fs->inode_buf) == 0) {
        return -ENOENT;
    }
    inode_to_node(fs, ino, fs->inode_buf, node_out);
    return 0;
}

typedef struct stream {
    const ext4_fs_t *fs;
    uint64_t pos;
    uint64_t end;
    ext4_data_cb cb;
    ext4_extent_cb map;
    void *ctx;
    bool stopped;
} stream_t;

static int stream_emit(stream_t *s, const void *data, size_t n) {
    int rc = s->cb(s->ctx, data, n);
    s->pos += n;
    if (rc > 0) {
        s->stopped = true;
        return 1;
    }
    return rc;
}

static int stream_zeros(stream_t *s, uint64_t upto) {
    if (upto > s->end) {
        upto = s->end;
    }
    if (s->pos >= upto) {
        return 0;
    }
    if (s->map) {
        s->pos = upto;
        return 0;
    }
    size_t first = upto - s->pos < STREAM_CHUNK ? (size_t)(upto - s->pos) : STREAM_CHUNK;
    memset(s->fs->stream_buf, 0, first);
    while (s->pos < upto) {
        size_t n = upto - s->pos < STREAM_CHUNK ? (size_t)(upto - s->pos) : STREAM_CHUNK;
        int rc = stream_emit(s, s->fs->stream_buf, n);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

static int stream_blocks(stream_t *s, uint64_t logical, uint64_t phys, uint64_t count, bool unwritten) {
    const ext4_fs_t *fs = s->fs;
    uint64_t start = logical * fs->block_size;
    uint64_t end = start + count * fs->block_size;
    if (start >= s->end) {
        return 1;
    }
    if (end <= s->pos) {
        return 0;
    }
    int rc = stream_zeros(s, start);
    if (rc != 0) {
        return rc;
    }
    if (end > s->end) {
        end = s->end;
    }
    if (unwritten) {
        return stream_zeros(s, end);
    }
    if (phys >= fs->total_blocks || count > fs->total_blocks - phys) {
        return -EIO;
    }
    if (s->map) {
        rc = s->map(s->ctx, s->pos, end - s->pos);
        s->pos = end;
        if (rc > 0) {
            s->stopped = true;
        }
        return rc;
    }
    uint64_t src = phys * fs->block_size;
    while (s->pos < end) {
        size_t n = end - s->pos < STREAM_CHUNK ? (size_t)(end - s->pos) : STREAM_CHUNK;
        rc = ext4_pread(fs, fs->stream_buf, n, src + (s->pos - start));
        if (rc < 0) {
            return rc;
        }
        rc = stream_emit(s, fs->stream_buf, n);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

static int stream_extent_node(stream_t *s, const uint8_t *node, size_t node_bytes, int expected_depth) {
    if (node_bytes < 12 || get_le16(node) != EXT4_EXTENT_MAGIC) {
        return -EIO;
    }
    uint16_t entries = get_le16(node + 2);
    uint16_t depth = get_le16(node + 6);
    if (12 + (size_t)entries * 12 > node_bytes || depth > EXT4_EXTENT_MAX_DEPTH ||
        (expected_depth >= 0 && depth != expected_depth)) {
        return -EIO;
    }
    if (depth == 0) {
        for (uint16_t i = 0; i < entries; i++) {
            const uint8_t *e = node + 12 + (size_t)i * 12;
            uint16_t raw_len = get_le16(e + 4);
            bool unwritten = raw_len > EXT4_EXTENT_INIT_MAX_LEN;
            uint64_t phys = (uint64_t)get_le32(e + 8) | ((uint64_t)get_le16(e + 6) << 32);
            int rc = stream_blocks(s, get_le32(e + 0), phys,
                                   unwritten ? raw_len - EXT4_EXTENT_INIT_MAX_LEN : raw_len, unwritten);
            if (rc != 0) {
                return rc;
            }
        }
        return 0;
    }
    uint8_t *child = malloc(s->fs->block_size);
    if (!child) {
        return -ENOMEM;
    }
    int rc = 0;
    for (uint16_t i = 0; i < entries && rc == 0; i++) {
        const uint8_t *idx = node + 12 + (size_t)i * 12;
        if ((uint64_t)get_le32(idx) * s->fs->block_size >= s->end) {
            rc = 1;
            break;
        }
        uint64_t leaf = (uint64_t)get_le32(idx + 4) | ((uint64_t)get_le16(idx + 8) << 32);
        if (leaf >= s->fs->total_blocks) {
            rc = -EIO;
            break;
        }
        rc = ext4_pread(s->fs, child, s->fs->block_size, leaf * s->fs->block_size);
        if (rc == 0) {
            rc = stream_extent_node(s, child, s->fs->block_size, depth - 1);
        }
    }
    free(child);
    return rc;
}

static int stream_indirect(stream_t *s, uint32_t block, unsigned level, uint64_t logical) {
    const ext4_fs_t *fs = s->fs;
    uint32_t per_block = fs->block_size / 4;
    if (block >= fs->total_blocks) {
        return -EIO;
    }
    uint8_t *pointers = malloc(fs->block_size);
    if (!pointers) {
        return -ENOMEM;
    }
    int rc = ext4_pread(fs, pointers, fs->block_size, (uint64_t)block * fs->block_size);
    uint64_t span = 1;
    for (unsigned l = 1; l < level; l++) {
        span *= per_block;
    }
    for (uint32_t i = 0; i < per_block && rc == 0; i++) {
        uint64_t child_logical = logical + (uint64_t)i * span;
        if (child_logical * fs->block_size >= s->end) {
            rc = 1;
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

static int stream_block_map(stream_t *s, const uint8_t *inode) {
    const ext4_fs_t *fs = s->fs;
    int rc = 0;
    for (uint32_t i = 0; i < EXT4_N_DIRECT_BLOCKS && rc == 0; i++) {
        uint32_t block = get_le32(inode + 0x28 + (size_t)i * 4);
        if ((uint64_t)i * fs->block_size >= s->end) {
            return 1;
        }
        if (block != 0) {
            rc = stream_blocks(s, i, block, 1, false);
        }
    }
    uint64_t logical = EXT4_N_DIRECT_BLOCKS;
    uint64_t span = fs->block_size / 4;
    for (unsigned level = 1; level <= 3 && rc == 0; level++) {
        if (logical * fs->block_size >= s->end) {
            return 1;
        }
        uint32_t block = get_le32(inode + 0x28 + (size_t)(EXT4_N_DIRECT_BLOCKS + level - 1) * 4);
        if (block != 0) {
            rc = stream_indirect(s, block, level, logical);
        }
        logical += span;
        span *= fs->block_size / 4;
    }
    return rc;
}

static int stream_inode(const ext4_fs_t *fs, const uint8_t *inode, uint64_t offset, uint64_t length,
                        ext4_data_cb cb, ext4_extent_cb map, void *ctx) {
    uint32_t flags = get_le32(inode + 0x20);
    if (flags & EXT4_INLINE_DATA_FL) {
        return -ENOTSUP;
    }
    uint64_t size = inode_size_field(inode);
    uint64_t end = offset >= size ? offset : (length > size - offset ? size : offset + length);
    stream_t s = {.fs = fs, .pos = offset, .end = end, .cb = cb, .map = map, .ctx = ctx};
    if (s.pos >= s.end) {
        return 0;
    }
    int rc = (flags & EXT4_EXTENTS_FL) ? stream_extent_node(&s, inode + 0x28, 60, -1) : stream_block_map(&s, inode);
    if (rc >= 0 && !s.stopped) {
        rc = stream_zeros(&s, s.end);
    }
    return rc < 0 ? rc : 0;
}

int ext4_fs_stream(ext4_fs_t *fs, const ext4_node_t *node, uint64_t offset, uint64_t length,
                   ext4_data_cb cb, void *ctx) {
    uint8_t *inode = malloc(fs->inode_size);
    if (!inode) {
        return -ENOMEM;
    }
    int rc = ext4_read_inode(fs, node->ino, inode);
    if (rc == 0) {
        rc = stream_inode(fs, inode, offset, length, cb, NULL, ctx);
    }
    free(inode);
    return rc;
}

int ext4_fs_map(ext4_fs_t *fs, const ext4_node_t *node, ext4_extent_cb cb, void *ctx) {
    uint8_t *inode = malloc(fs->inode_size);
    if (!inode) {
        return -ENOMEM;
    }
    int rc = ext4_read_inode(fs, node->ino, inode);
    if (rc == 0) {
        rc = stream_inode(fs, inode, 0, UINT64_MAX, NULL, cb, ctx);
    }
    free(inode);
    return rc;
}

typedef struct byte_buffer {
    uint8_t *data;
    size_t len;
    size_t cap;
} byte_buffer_t;

static int byte_buffer_collect(void *ctx, const void *data, size_t len) {
    byte_buffer_t *b = ctx;
    if (len > b->cap - b->len) {
        return -EIO;
    }
    memcpy(b->data + b->len, data, len);
    b->len += len;
    return 0;
}

static int read_inode_data(const ext4_fs_t *fs, const uint8_t *inode, size_t size, uint8_t **data_out) {
    byte_buffer_t b = {.data = malloc(size > 0 ? size : 1), .cap = size};
    if (!b.data) {
        return -ENOMEM;
    }
    int rc = stream_inode(fs, inode, 0, size, byte_buffer_collect, NULL, &b);
    if (rc == 0 && b.len != size) {
        rc = -EIO;
    }
    if (rc < 0) {
        free(b.data);
        return rc;
    }
    *data_out = b.data;
    return 0;
}

int ext4_fs_read(ext4_fs_t *fs, const ext4_node_t *node, size_t limit, uint8_t **data_out, size_t *size_out) {
    if (node->size > limit) {
        return -EFBIG;
    }
    uint8_t *inode = malloc(fs->inode_size);
    if (!inode) {
        return -ENOMEM;
    }
    int rc = ext4_read_inode(fs, node->ino, inode);
    if (rc == 0) {
        rc = read_inode_data(fs, inode, (size_t)node->size, data_out);
    }
    free(inode);
    if (rc == 0) {
        *size_out = (size_t)node->size;
    }
    return rc;
}

static int read_small_inode(const ext4_fs_t *fs, uint32_t ino, uint64_t expected_size, uint8_t **data_out) {
    uint8_t *inode = malloc(fs->inode_size);
    if (!inode) {
        return -ENOMEM;
    }
    int rc = ext4_read_inode(fs, ino, inode);
    if (rc == 0 && inode_size_field(inode) != expected_size) {
        rc = -EIO;
    }
    if (rc == 0) {
        rc = read_inode_data(fs, inode, (size_t)expected_size, data_out);
    }
    free(inode);
    return rc;
}

typedef struct xattr_list {
    ext4_xattr_info_t *items;
    size_t count;
    size_t cap;
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
    if (l->count == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 4;
        ext4_xattr_info_t *items = realloc(l->items, cap * sizeof(*items));
        if (!items) {
            return -ENOMEM;
        }
        l->items = items;
        l->cap = cap;
    }
    size_t prefix_len = strlen(prefix);
    char *blob = malloc(prefix_len + suffix_len + 1 + size);
    if (!blob) {
        return -ENOMEM;
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
        return -EIO;
    }
    size_t max_entries = (size - 4) / 4;
    uint8_t *posix = malloc(4 + max_entries * 8);
    if (!posix) {
        return -ENOMEM;
    }
    put_le32(posix, POSIX_ACL_XATTR_VERSION);
    size_t out = 4;
    size_t off = 4;
    int rc = 0;
    while (off < size) {
        if (off + 4 > size) {
            rc = -EIO;
            break;
        }
        uint16_t tag = get_le16(value + off);
        uint32_t id = EXT4_ACL_UNDEFINED_ID;
        size_t entry_size = 4;
        if (tag == EXT4_ACL_USER || tag == EXT4_ACL_GROUP) {
            if (off + 8 > size) {
                rc = -EIO;
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

static int parse_xattrs(const ext4_fs_t *fs, const uint8_t *values, const uint8_t *entries,
                        const uint8_t *end, xattr_list_t *l) {
    const uint8_t *p = entries;
    while (p + 4 <= end && get_le32(p) != 0) {
        if (p + EXT4_XATTR_ENTRY_SIZE > end) {
            return -EIO;
        }
        uint8_t name_len = p[0];
        uint8_t index = p[1];
        uint16_t value_offs = get_le16(p + 2);
        uint32_t value_inum = get_le32(p + 4);
        uint32_t value_size = get_le32(p + 8);
        size_t entry_len = (EXT4_XATTR_ENTRY_SIZE + (size_t)name_len + 3U) & ~(size_t)3U;
        if (p + entry_len > end) {
            return -EIO;
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
        if (value_size > EXT4_XATTR_VALUE_MAX) {
            return -EIO;
        }

        uint8_t *owned = NULL;
        const uint8_t *value = NULL;
        if (value_inum != 0) {
            int rc = read_small_inode(fs, value_inum, value_size, &owned);
            if (rc < 0) {
                return rc;
            }
            value = owned;
        } else {
            if ((size_t)value_offs + value_size > (size_t)(end - values)) {
                return -EIO;
            }
            value = values + value_offs;
        }

        int rc;
        if (index == EXT4_XATTR_INDEX_POSIX_ACL_ACCESS || index == EXT4_XATTR_INDEX_POSIX_ACL_DEFAULT) {
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

static int read_xattrs(const ext4_fs_t *fs, const uint8_t *inode, xattr_list_t *l) {
    if (fs->inode_size > EXT4_GOOD_OLD_INODE_SIZE) {
        size_t ibody = EXT4_GOOD_OLD_INODE_SIZE + get_le16(inode + 0x80);
        if (ibody + 4 <= fs->inode_size && get_le32(inode + ibody) == EXT4_XATTR_MAGIC) {
            int rc = parse_xattrs(fs, inode + ibody + 4, inode + ibody + 4, inode + fs->inode_size, l);
            if (rc < 0) {
                return rc;
            }
        }
    }
    uint64_t block = (uint64_t)get_le32(inode + 0x68) | ((uint64_t)get_le16(inode + 0x76) << 32);
    if (block == 0) {
        return 0;
    }
    if (block >= fs->total_blocks) {
        return -EIO;
    }
    uint8_t *buf = malloc(fs->block_size);
    if (!buf) {
        return -ENOMEM;
    }
    int rc = ext4_pread(fs, buf, fs->block_size, block * fs->block_size);
    if (rc == 0 && (get_le32(buf) != EXT4_XATTR_MAGIC || get_le32(buf + 8) != 1)) {
        rc = -EIO;
    }
    if (rc == 0) {
        rc = parse_xattrs(fs, buf, buf + EXT4_XATTR_BLOCK_HEADER_SIZE, buf + fs->block_size, l);
    }
    free(buf);
    return rc;
}

static int read_symlink(const ext4_fs_t *fs, const uint8_t *inode, char **target_out) {
    uint64_t size = inode_size_field(inode);
    if (size == 0 || size >= EXT4_SYMLINK_MAX) {
        return -EIO;
    }
    uint32_t flags = get_le32(inode + 0x20);
    char *target;
    if (size < EXT4_FAST_SYMLINK_MAX && !(flags & (EXT4_EXTENTS_FL | EXT4_INLINE_DATA_FL))) {
        target = malloc((size_t)size + 1);
        if (!target) {
            return -ENOMEM;
        }
        memcpy(target, inode + 0x28, (size_t)size);
    } else {
        uint8_t *raw = NULL;
        int rc = read_inode_data(fs, inode, (size_t)size, &raw);
        if (rc < 0) {
            return rc;
        }
        target = realloc(raw, (size_t)size + 1);
        if (!target) {
            free(raw);
            return -ENOMEM;
        }
    }
    target[size] = '\0';
    if (strlen(target) != size) {
        free(target);
        return -EIO;
    }
    *target_out = target;
    return 0;
}

typedef struct dir_parse {
    uint32_t block_size;
    ext4_list_cb cb;
    void *ctx;
} dir_parse_t;

static size_t dirent_rec_len(uint16_t raw, uint32_t block_size) {
    if (raw == 0xFFFF || raw == 0) {
        return block_size;
    }
    return (size_t)(raw & 0xFFFCU) | ((size_t)(raw & 3U) << 16);
}

static int dir_parse_chunk(void *ctx, const void *data, size_t len) {
    dir_parse_t *d = ctx;
    const uint8_t *chunk = data;
    for (size_t block = 0; block < len; block += d->block_size) {
        size_t block_len = len - block < d->block_size ? len - block : d->block_size;
        const uint8_t *p = chunk + block;
        size_t off = 0;
        while (off + 8 <= block_len) {
            uint32_t ino = get_le32(p + off);
            size_t rec_len = dirent_rec_len(get_le16(p + off + 4), d->block_size);
            uint8_t name_len = p[off + 6];
            if (rec_len < 8 || off + rec_len > block_len || (size_t)name_len + 8 > rec_len) {
                return -EIO;
            }
            const char *name = (const char *)p + off + 8;
            bool dot = (name_len == 1 && name[0] == '.') || (name_len == 2 && name[0] == '.' && name[1] == '.');
            if (ino != 0 && name_len != 0 && !dot) {
                int rc = d->cb(d->ctx, name, name_len, ino);
                if (rc != 0) {
                    return rc;
                }
            }
            off += rec_len;
        }
    }
    return 0;
}

static int list_inode(ext4_fs_t *fs, uint32_t ino, ext4_list_cb cb, void *ctx) {
    uint8_t *inode = malloc(fs->inode_size);
    if (!inode) {
        return -ENOMEM;
    }
    int rc = ext4_read_inode(fs, ino, inode);
    if (rc == 0 && !S_ISDIR(get_le16(inode))) {
        rc = -ENOTDIR;
    }
    if (rc == 0) {
        dir_parse_t d = {.block_size = fs->block_size, .cb = cb, .ctx = ctx};
        rc = stream_inode(fs, inode, 0, UINT64_MAX, dir_parse_chunk, NULL, &d);
    }
    free(inode);
    return rc;
}

int ext4_fs_list(ext4_fs_t *fs, const ext4_node_t *dir, ext4_list_cb cb, void *ctx) {
    return list_inode(fs, dir->ino, cb, ctx);
}

typedef struct lookup {
    const char *name;
    size_t name_len;
    uint32_t ino;
} lookup_t;

static int lookup_match(void *ctx, const char *name, size_t name_len, uint32_t ino) {
    lookup_t *l = ctx;
    if (name_len == l->name_len && memcmp(name, l->name, name_len) == 0) {
        l->ino = ino;
        return 1;
    }
    return 0;
}

static int lookup_child(ext4_fs_t *fs, uint32_t dir_ino, const char *name, size_t name_len, uint32_t *ino_out) {
    lookup_t l = {.name = name, .name_len = name_len};
    int rc = list_inode(fs, dir_ino, lookup_match, &l);
    if (rc < 0) {
        return rc;
    }
    if (l.ino == 0) {
        return -ENOENT;
    }
    *ino_out = l.ino;
    return 0;
}

int ext4_fs_resolve(ext4_fs_t *fs, const char *path, bool follow_final, ext4_node_t *node_out) {
    uint32_t *stack = malloc(RESOLVE_MAX_DEPTH * sizeof(*stack));
    char *rest = strdup(path);
    if (!stack || !rest) {
        free(stack);
        free(rest);
        return -ENOMEM;
    }
    size_t depth = 1;
    stack[0] = EXT4_ROOT_INO;
    unsigned hops = 0;
    size_t pos = 0;
    int rc = 0;
    ext4_node_t current;
    rc = ext4_fs_stat(fs, EXT4_ROOT_INO, &current);
    while (rc == 0) {
        while (rest[pos] == '/') {
            pos++;
        }
        if (rest[pos] == '\0') {
            break;
        }
        size_t len = strcspn(rest + pos, "/");
        const char *name = rest + pos;
        size_t next = pos + len;
        bool final = true;
        for (size_t i = next; rest[i] != '\0'; i++) {
            if (rest[i] != '/') {
                final = false;
                break;
            }
        }
        if (!S_ISDIR(current.mode)) {
            rc = -ENOTDIR;
            break;
        }
        if (len == 1 && name[0] == '.') {
            pos = next;
            continue;
        }
        if (len == 2 && name[0] == '.' && name[1] == '.') {
            if (depth > 1) {
                depth--;
            }
            rc = ext4_fs_stat(fs, stack[depth - 1], &current);
            pos = next;
            continue;
        }
        if (len > 255) {
            rc = -ENAMETOOLONG;
            break;
        }
        uint32_t ino = 0;
        rc = lookup_child(fs, stack[depth - 1], name, len, &ino);
        ext4_node_t child;
        if (rc == 0) {
            rc = ext4_fs_stat(fs, ino, &child);
        }
        if (rc < 0) {
            break;
        }
        if (S_ISLNK(child.mode) && (!final || follow_final)) {
            if (++hops > RESOLVE_MAX_SYMLINKS) {
                rc = -ELOOP;
                break;
            }
            char *target = NULL;
            rc = read_symlink(fs, fs->inode_buf, &target);
            if (rc < 0) {
                break;
            }
            size_t target_len = strlen(target);
            size_t tail_len = strlen(rest + next);
            char *joined = malloc(target_len + 1 + tail_len + 1);
            if (!joined) {
                free(target);
                rc = -ENOMEM;
                break;
            }
            memcpy(joined, target, target_len);
            joined[target_len] = '/';
            memcpy(joined + target_len + 1, rest + next, tail_len + 1);
            if (target[0] == '/') {
                depth = 1;
            }
            free(target);
            free(rest);
            rest = joined;
            pos = 0;
            rc = ext4_fs_stat(fs, stack[depth - 1], &current);
            continue;
        }
        if (depth == RESOLVE_MAX_DEPTH) {
            rc = -ENAMETOOLONG;
            break;
        }
        stack[depth++] = ino;
        current = child;
        pos = next;
    }
    free(stack);
    free(rest);
    if (rc == 0) {
        *node_out = current;
    }
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
} dir_listing_t;

static void dir_listing_free(dir_listing_t *d) {
    free(d->items);
    free(d->names);
    *d = (dir_listing_t){0};
}

static int dir_listing_add(void *ctx, const char *name, size_t name_len, uint32_t ino) {
    dir_listing_t *d = ctx;
    if (d->count == d->cap) {
        size_t cap = d->cap ? d->cap * 2 : 32;
        struct dir_item *items = realloc(d->items, cap * sizeof(*items));
        if (!items) {
            return -ENOMEM;
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
            return -ENOMEM;
        }
        d->names = names;
        d->names_cap = cap;
    }
    if (d->names_len > UINT32_MAX - name_len) {
        return -ENOMEM;
    }
    memcpy(d->names + d->names_len, name, name_len);
    d->items[d->count++] = (struct dir_item){
        .ino = ino,
        .name_off = (uint32_t)d->names_len,
        .name_len = (uint8_t)name_len,
    };
    d->names_len += name_len;
    return 0;
}

static void decode_rdev(const uint8_t *iblock, uint64_t *major, uint64_t *minor) {
    uint32_t old_dev = get_le32(iblock + 0);
    if (old_dev != 0) {
        *major = (old_dev >> 8) & 0xFFU;
        *minor = old_dev & 0xFFU;
        return;
    }
    uint32_t new_dev = get_le32(iblock + 4);
    *major = (new_dev >> 8) & 0xFFFU;
    *minor = (new_dev & 0xFFU) | ((new_dev >> 12) & 0xFFF00U);
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
            return -ENOMEM;
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

static int visit_entry(ext4_fs_t *fs, uint32_t ino, const char *path, uint8_t *inode,
                       ext4_walk_cb cb, void *ctx, uint32_t *mode_out) {
    int rc = ext4_read_inode(fs, ino, inode);
    if (rc < 0) {
        return rc;
    }
    ext4_entry_info_t info = {0};
    inode_to_node(fs, ino, inode, &info.node);
    info.mtime = inode_time(fs, inode, 0x10, 0x88, &info.mtime_nsec);
    *mode_out = info.node.mode;

    xattr_list_t xattrs = {0};
    rc = read_xattrs(fs, inode, &xattrs);
    char *symlink_target = NULL;
    if (rc == 0 && S_ISLNK(info.node.mode)) {
        rc = read_symlink(fs, inode, &symlink_target);
        info.symlink_target = symlink_target;
    } else if (S_ISCHR(info.node.mode) || S_ISBLK(info.node.mode)) {
        decode_rdev(inode + 0x28, &info.rdev_major, &info.rdev_minor);
    }
    if (rc == 0) {
        info.xattrs = xattrs.items;
        info.xattr_count = xattrs.count;
        rc = cb(ctx, path, &info);
    }
    free(symlink_target);
    xattr_list_free(&xattrs);
    return rc;
}

static char *join_path(const char *parent, const char *name, size_t name_len) {
    bool is_root = parent[0] == '.' && parent[1] == '\0';
    size_t plen = is_root ? 0 : strlen(parent) + 1;
    char *path = malloc(plen + name_len + 1);
    if (!path) {
        return NULL;
    }
    if (!is_root) {
        memcpy(path, parent, plen - 1);
        path[plen - 1] = '/';
    }
    memcpy(path + plen, name, name_len);
    path[plen + name_len] = '\0';
    return path;
}

int ext4_fs_walk(ext4_fs_t *fs, ext4_walk_cb cb, void *ctx) {
    typedef struct walk_frame {
        dir_listing_t listing;
        size_t next;
        char *path;
    } walk_frame_t;

    uint8_t *inode = malloc(fs->inode_size);
    size_t frames_cap = 16;
    walk_frame_t *frames = calloc(frames_cap, sizeof(*frames));
    char *root_path = strdup(".");
    if (!inode || !frames || !root_path) {
        free(inode);
        free(frames);
        free(root_path);
        return -ENOMEM;
    }
    size_t depth = 0;
    ino_set_t visited = {0};
    uint32_t mode = 0;
    int rc = ino_set_insert(&visited, EXT4_ROOT_INO);
    if (rc >= 0) {
        rc = visit_entry(fs, EXT4_ROOT_INO, root_path, inode, cb, ctx, &mode);
    }
    if (rc == 0) {
        rc = list_inode(fs, EXT4_ROOT_INO, dir_listing_add, &frames[0].listing);
    }
    if (rc == 0) {
        frames[0].path = root_path;
        root_path = NULL;
        depth = 1;
    }
    free(root_path);
    while (rc == 0 && depth > 0) {
        walk_frame_t *frame = &frames[depth - 1];
        if (frame->next == frame->listing.count) {
            dir_listing_free(&frame->listing);
            free(frame->path);
            frame->path = NULL;
            depth--;
            continue;
        }
        const struct dir_item *item = &frame->listing.items[frame->next++];
        char *path = join_path(frame->path, frame->listing.names + item->name_off, item->name_len);
        if (!path) {
            rc = -ENOMEM;
            break;
        }
        rc = visit_entry(fs, item->ino, path, inode, cb, ctx, &mode);
        if (rc < 0 || !S_ISDIR(mode)) {
            free(path);
            continue;
        }
        int inserted = ino_set_insert(&visited, item->ino);
        if (inserted <= 0) {
            free(path);
            rc = inserted < 0 ? inserted : -EIO;
            continue;
        }
        if (depth == frames_cap) {
            walk_frame_t *grown = realloc(frames, frames_cap * 2 * sizeof(*frames));
            if (!grown) {
                free(path);
                rc = -ENOMEM;
                continue;
            }
            frames = grown;
            frames_cap *= 2;
        }
        frames[depth] = (walk_frame_t){.path = path};
        rc = list_inode(fs, item->ino, dir_listing_add, &frames[depth].listing);
        depth++;
    }
    while (depth > 0) {
        depth--;
        dir_listing_free(&frames[depth].listing);
        free(frames[depth].path);
    }
    free(visited.slots);
    free(frames);
    free(inode);
    return rc;
}
