#define _DARWIN_C_SOURCE

#include "host_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum : uint32_t {
    BLOCK_SIZE = 4096,
    BLOCKS_PER_GROUP = 32768,
    INODES_PER_GROUP = 8192,
    INODE_SIZE = 256,
    INODE_EXTRA_ISIZE = 32,
    INODE_TABLE_BLOCKS = INODES_PER_GROUP * INODE_SIZE / BLOCK_SIZE,
    DESC_SIZE = 64,
    LOG_GROUPS_PER_FLEX = 4,
    GROUPS_PER_FLEX = 1U << LOG_GROUPS_PER_FLEX,
    MIN_BLOCKS = 16384,
    LAST_GROUP_SLACK = 256,

    ROOT_INO = 2,
    JOURNAL_INO = 8,
    FIRST_INO = 11,
    LOST_FOUND_BLOCKS = 4,
    LINK_MAX = 65000,

    EXT4_SUPER_MAGIC = 0xEF53,
    EXT4_EXTENTS_FL = 0x00080000,
    EXT4_EXTENT_MAGIC = 0xF30A,
    EXTENT_MAX_LEN = 32768,
    EXTENTS_IN_INODE = 4,
    EXTENTS_PER_BLOCK = (BLOCK_SIZE - 12) / 12,
    XATTR_MAGIC = 0xEA020000,
    XATTR_BLOCK_HEADER = 32,
    XATTR_ENTRY_HEADER = 16,
    XATTR_IBODY_OFFSET = 128 + INODE_EXTRA_ISIZE,
    XATTR_IBODY_SPACE = INODE_SIZE - XATTR_IBODY_OFFSET - 4,
    FAST_SYMLINK_MAX = 60,

    COMPAT_HAS_JOURNAL = 0x0004,
    COMPAT_EXT_ATTR = 0x0008,
    COMPAT_DIR_INDEX = 0x0020,
    INCOMPAT_FILETYPE = 0x0002,
    INCOMPAT_EXTENTS = 0x0040,
    INCOMPAT_64BIT = 0x0080,
    INCOMPAT_FLEX_BG = 0x0200,
    RO_COMPAT_SPARSE_SUPER = 0x0001,
    RO_COMPAT_LARGE_FILE = 0x0002,
    RO_COMPAT_HUGE_FILE = 0x0008,
    RO_COMPAT_DIR_NLINK = 0x0020,
    RO_COMPAT_EXTRA_ISIZE = 0x0040,
    DEFM_XATTR_USER = 0x0004,
    DEFM_ACL = 0x0008,
    FLAGS_UNSIGNED_HASH = 0x0002,
    HASH_HALF_MD4 = 1,
    JNL_BACKUP_BLOCKS = 1,

    JBD2_MAGIC = 0xC03B3998,
    JBD2_SUPERBLOCK_V2 = 4,

    STAGE_SIZE = 1U << 20,
    HOLE_MIN_BLOCKS = 16,
};

enum : uint8_t {
    FT_REG = 1,
    FT_DIR = 2,
    FT_CHR = 3,
    FT_BLK = 4,
    FT_FIFO = 5,
    FT_SOCK = 6,
    FT_SYMLINK = 7,
};

typedef struct block_range {
    uint64_t start;
    uint64_t end;
} block_range_t;

typedef struct group_layout {
    uint64_t block_bitmap;
    uint64_t inode_bitmap;
    uint64_t inode_table;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t dirs;
} group_layout_t;

struct ext4_writer {
    int fd;
    uint64_t total_blocks;
    uint32_t groups;
    uint32_t gdt_blocks;
    uint32_t journal_blocks;
    uint64_t journal_start;
    group_layout_t *layout;

    block_range_t *reserved;
    size_t reserved_count;
    size_t reserved_next;
    uint64_t cursor;

    block_range_t *freed;
    size_t freed_count;
    size_t freed_cap;

    vfs_node_t **files;
    size_t file_count;
    size_t file_cap;

    vfs_node_t *file;
    uint64_t file_pos;
    uint8_t *stage;
    uint64_t stage_block;
    size_t stage_len;

    vfs_node_t *lost_found;
    uint8_t uuid[16];
    uint8_t hash_seed[16];
    int64_t now;
};

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

static inline uint32_t get_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline size_t align4(size_t v) {
    return (v + 3U) & ~(size_t)3U;
}

static int pwrite_all(int fd, const void *buf, size_t len, uint64_t off) {
    const uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = pwrite(fd, p, len, (off_t)off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -errno;
        }
        p += (size_t)n;
        len -= (size_t)n;
        off += (uint64_t)n;
    }
    return 0;
}

static int write_blocks(ext4_writer_t *w, uint64_t block, const void *data, size_t len) {
    return pwrite_all(w->fd, data, len, block * BLOCK_SIZE);
}

static bool is_power_of(uint32_t n, uint32_t base) {
    while (n > 1 && n % base == 0) {
        n /= base;
    }
    return n == 1;
}

static bool group_has_super(uint32_t group) {
    if (group <= 1) {
        return true;
    }
    if ((group & 1U) == 0) {
        return false;
    }
    return is_power_of(group, 3) || is_power_of(group, 5) || is_power_of(group, 7);
}

static uint64_t group_first_block(uint32_t group) {
    return (uint64_t)group * BLOCKS_PER_GROUP;
}

static uint64_t group_block_count(const ext4_writer_t *w, uint32_t group) {
    uint64_t start = group_first_block(group);
    uint64_t left = w->total_blocks - start;
    return left < BLOCKS_PER_GROUP ? left : BLOCKS_PER_GROUP;
}

static uint32_t journal_size_for(uint64_t blocks) {
    if (blocks < 32768) {
        return 1024;
    }
    if (blocks < 256 * 1024) {
        return 4096;
    }
    if (blocks < 512 * 1024) {
        return 8192;
    }
    return 16384;
}

static uint64_t flex_metadata_blocks(uint32_t groups_in_flex) {
    return (uint64_t)groups_in_flex * (2 + INODE_TABLE_BLOCKS);
}

static void compute_geometry(uint64_t total_blocks, uint32_t *groups_out, uint32_t *gdt_blocks_out) {
    uint32_t groups = (uint32_t)((total_blocks + BLOCKS_PER_GROUP - 1) / BLOCKS_PER_GROUP);
    *groups_out = groups;
    *gdt_blocks_out = (uint32_t)(((uint64_t)groups * DESC_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE);
}

static uint64_t group_overhead(uint32_t group, uint32_t groups, uint32_t gdt_blocks) {
    uint64_t overhead = group_has_super(group) ? 1U + gdt_blocks : 0U;
    if (group % GROUPS_PER_FLEX == 0) {
        uint32_t in_flex = groups - group < GROUPS_PER_FLEX ? groups - group : GROUPS_PER_FLEX;
        overhead += flex_metadata_blocks(in_flex);
    }
    return overhead;
}

static int add_reserved(ext4_writer_t *w, uint64_t start, uint64_t end) {
    block_range_t *ranges = realloc(w->reserved, (w->reserved_count + 1) * sizeof(*ranges));
    if (!ranges) {
        return -ENOMEM;
    }
    w->reserved = ranges;
    w->reserved[w->reserved_count++] = (block_range_t){.start = start, .end = end};
    return 0;
}

static int compare_ranges(const void *a, const void *b) {
    const block_range_t *x = a;
    const block_range_t *y = b;
    return x->start < y->start ? -1 : x->start > y->start;
}

static int plan_layout(ext4_writer_t *w) {
    uint32_t groups = 0;
    uint32_t gdt_blocks = 0;
    for (int pass = 0; pass < 3; pass++) {
        compute_geometry(w->total_blocks, &groups, &gdt_blocks);
        uint32_t last = groups - 1;
        uint64_t last_size = w->total_blocks - group_first_block(last);
        if (groups == 1 || last_size >= group_overhead(last, groups, gdt_blocks) + LAST_GROUP_SLACK) {
            break;
        }
        w->total_blocks = group_first_block(last);
    }
    w->groups = groups;
    w->gdt_blocks = gdt_blocks;
    w->journal_blocks = journal_size_for(w->total_blocks);
    w->layout = calloc(groups, sizeof(*w->layout));
    if (!w->layout) {
        return -ENOMEM;
    }

    for (uint32_t g = 0; g < groups; g++) {
        if (group_has_super(g)) {
            int rc = add_reserved(w, group_first_block(g), group_first_block(g) + 1 + gdt_blocks);
            if (rc < 0) {
                return rc;
            }
        }
    }
    for (uint32_t first = 0; first < groups; first += GROUPS_PER_FLEX) {
        uint32_t in_flex = groups - first < GROUPS_PER_FLEX ? groups - first : GROUPS_PER_FLEX;
        uint64_t base = group_first_block(first) + (group_has_super(first) ? 1U + gdt_blocks : 0U);
        uint64_t end = base + flex_metadata_blocks(in_flex);
        if (end > group_first_block(first) + group_block_count(w, first)) {
            return -ENOSPC;
        }
        for (uint32_t i = 0; i < in_flex; i++) {
            w->layout[first + i].block_bitmap = base + i;
            w->layout[first + i].inode_bitmap = base + in_flex + i;
            w->layout[first + i].inode_table = base + 2ULL * in_flex + (uint64_t)i * INODE_TABLE_BLOCKS;
        }
        int rc = add_reserved(w, base, end);
        if (rc < 0) {
            return rc;
        }
        if (first == 0) {
            w->journal_start = end;
        }
    }
    if (w->journal_start + w->journal_blocks > group_block_count(w, 0)) {
        return -ENOSPC;
    }
    int rc = add_reserved(w, w->journal_start, w->journal_start + w->journal_blocks);
    if (rc < 0) {
        return rc;
    }
    qsort(w->reserved, w->reserved_count, sizeof(*w->reserved), compare_ranges);
    return 0;
}

int ext4_writer_create(int fd, uint64_t disk_size, ext4_writer_t **writer_out) {
    *writer_out = NULL;
    uint64_t total_blocks = disk_size / BLOCK_SIZE;
    if (total_blocks < MIN_BLOCKS || total_blocks > UINT32_MAX) {
        return -EINVAL;
    }
    ext4_writer_t *w = calloc(1, sizeof(*w));
    if (!w) {
        return -ENOMEM;
    }
    w->fd = fd;
    w->total_blocks = total_blocks;
    w->now = (int64_t)time(NULL);
    w->stage = malloc(STAGE_SIZE);
    int rc = w->stage ? plan_layout(w) : -ENOMEM;
    if (rc == 0 && ftruncate(fd, (off_t)(w->total_blocks * BLOCK_SIZE)) != 0) {
        rc = -errno;
    }
    if (rc < 0) {
        ext4_writer_free(w);
        return rc;
    }
    arc4random_buf(w->uuid, sizeof(w->uuid));
    w->uuid[6] = (uint8_t)((w->uuid[6] & 0x0F) | 0x40);
    w->uuid[8] = (uint8_t)((w->uuid[8] & 0x3F) | 0x80);
    arc4random_buf(w->hash_seed, sizeof(w->hash_seed));
    *writer_out = w;
    return 0;
}

void ext4_writer_free(ext4_writer_t *w) {
    if (!w) {
        return;
    }
    for (size_t i = 0; i < w->file_count; i++) {
        vfs_node_release(w->files[i]);
    }
    free(w->files);
    free(w->freed);
    free(w->reserved);
    free(w->layout);
    free(w->stage);
    free(w);
}

static int alloc_blocks(ext4_writer_t *w, uint64_t want, uint64_t *phys_out, uint32_t *got_out) {
    for (;;) {
        while (w->reserved_next < w->reserved_count && w->reserved[w->reserved_next].end <= w->cursor) {
            w->reserved_next++;
        }
        if (w->cursor >= w->total_blocks) {
            return -ENOSPC;
        }
        if (w->reserved_next < w->reserved_count && w->reserved[w->reserved_next].start <= w->cursor) {
            w->cursor = w->reserved[w->reserved_next].end;
            continue;
        }
        uint64_t limit = w->reserved_next < w->reserved_count ? w->reserved[w->reserved_next].start : w->total_blocks;
        uint64_t got = limit - w->cursor;
        if (got > want) {
            got = want;
        }
        if (got > EXTENT_MAX_LEN) {
            got = EXTENT_MAX_LEN;
        }
        *phys_out = w->cursor;
        *got_out = (uint32_t)got;
        w->cursor += got;
        return 0;
    }
}

static int node_add_extent(vfs_node_t *node, uint64_t logical, uint64_t physical, uint32_t length) {
    if (node->extent_count > 0) {
        vfs_extent_t *last = &node->extents[node->extent_count - 1];
        if (last->logical + last->length == logical && last->physical + last->length == physical &&
            (uint64_t)last->length + length <= EXTENT_MAX_LEN) {
            last->length += length;
            return 0;
        }
    }
    if (node->extent_count == node->extent_cap) {
        size_t cap = node->extent_cap ? node->extent_cap * 2 : 2;
        vfs_extent_t *extents = realloc(node->extents, cap * sizeof(*extents));
        if (!extents) {
            return -ENOMEM;
        }
        node->extents = extents;
        node->extent_cap = cap;
    }
    node->extents[node->extent_count++] = (vfs_extent_t){
        .logical = logical,
        .physical = physical,
        .length = length,
    };
    return 0;
}

static int write_extent_data(ext4_writer_t *w, vfs_node_t *node, uint64_t logical, const uint8_t *data,
                             uint64_t blocks) {
    while (blocks > 0) {
        uint64_t phys = 0;
        uint32_t got = 0;
        int rc = alloc_blocks(w, blocks, &phys, &got);
        if (rc == 0) {
            rc = write_blocks(w, phys, data, (size_t)got * BLOCK_SIZE);
        }
        if (rc == 0) {
            rc = node_add_extent(node, logical, phys, got);
        }
        if (rc < 0) {
            return rc;
        }
        logical += got;
        data += (size_t)got * BLOCK_SIZE;
        blocks -= got;
    }
    return 0;
}

static int flush_stage(ext4_writer_t *w, bool final) {
    size_t full = w->stage_len / BLOCK_SIZE;
    size_t tail = w->stage_len % BLOCK_SIZE;
    size_t blocks = full;
    if (final && tail > 0) {
        memset(w->stage + w->stage_len, 0, BLOCK_SIZE - tail);
        blocks++;
        tail = 0;
    }
    if (w->stage_block + blocks > (1ULL << 32)) {
        return -EFBIG;
    }
    int rc = write_extent_data(w, w->file, w->stage_block, w->stage, blocks);
    if (rc < 0) {
        return rc;
    }
    if (tail > 0) {
        memmove(w->stage, w->stage + full * BLOCK_SIZE, tail);
    }
    w->stage_block += blocks;
    w->stage_len = tail;
    return 0;
}

static int stage_append(ext4_writer_t *w, const uint8_t *data, uint64_t length) {
    while (length > 0) {
        size_t n = STAGE_SIZE - w->stage_len;
        if (n > length) {
            n = (size_t)length;
        }
        if (data) {
            memcpy(w->stage + w->stage_len, data, n);
            data += n;
        } else {
            memset(w->stage + w->stage_len, 0, n);
        }
        w->stage_len += n;
        w->file_pos += n;
        length -= n;
        if (w->stage_len == STAGE_SIZE) {
            int rc = flush_stage(w, false);
            if (rc < 0) {
                return rc;
            }
        }
    }
    return 0;
}

static int stage_hole(ext4_writer_t *w, uint64_t offset) {
    uint64_t in_block = w->file_pos % BLOCK_SIZE;
    if (in_block > 0) {
        uint64_t fill = BLOCK_SIZE - in_block;
        if (fill > offset - w->file_pos) {
            fill = offset - w->file_pos;
        }
        int rc = stage_append(w, NULL, fill);
        if (rc < 0) {
            return rc;
        }
    }
    uint64_t skip_blocks = (offset - w->file_pos) / BLOCK_SIZE;
    if (skip_blocks >= HOLE_MIN_BLOCKS) {
        int rc = flush_stage(w, false);
        if (rc < 0) {
            return rc;
        }
        w->file_pos += skip_blocks * BLOCK_SIZE;
        w->stage_block = w->file_pos / BLOCK_SIZE;
    }
    return stage_append(w, NULL, offset - w->file_pos);
}

static int track_file(ext4_writer_t *w, vfs_node_t *file) {
    if (w->file_count == w->file_cap) {
        size_t cap = w->file_cap ? w->file_cap * 2 : 1024;
        vfs_node_t **files = realloc(w->files, cap * sizeof(*files));
        if (!files) {
            return -ENOMEM;
        }
        w->files = files;
        w->file_cap = cap;
    }
    w->files[w->file_count++] = vfs_node_retain(file);
    return 0;
}

int ext4_writer_write(ext4_writer_t *w, vfs_node_t *file, uint64_t offset, const void *data, size_t length) {
    if (w->file != file) {
        if (w->file || file->extent_count > 0) {
            return -EINVAL;
        }
        int rc = track_file(w, file);
        if (rc < 0) {
            return rc;
        }
        w->file = file;
        w->file_pos = 0;
        w->stage_block = 0;
        w->stage_len = 0;
    }
    if (offset < w->file_pos) {
        return -EINVAL;
    }
    if (offset > w->file_pos) {
        int rc = stage_hole(w, offset);
        if (rc < 0) {
            return rc;
        }
    }
    return stage_append(w, data, length);
}

int ext4_writer_end_file(ext4_writer_t *w, vfs_node_t *file, uint64_t size) {
    if (w->file != file) {
        file->size = size;
        return 0;
    }
    if (size < w->file_pos) {
        return -EINVAL;
    }
    int rc = flush_stage(w, true);
    w->file = NULL;
    file->size = size;
    return rc;
}

static uint8_t file_type(uint32_t mode) {
    switch (mode & S_IFMT) {
    case S_IFREG: return FT_REG;
    case S_IFDIR: return FT_DIR;
    case S_IFCHR: return FT_CHR;
    case S_IFBLK: return FT_BLK;
    case S_IFIFO: return FT_FIFO;
    case S_IFSOCK: return FT_SOCK;
    case S_IFLNK: return FT_SYMLINK;
    default: return 0;
    }
}

typedef struct numbering {
    vfs_node_t **by_ino;
    uint32_t *parent;
    uint32_t last_ino;
} numbering_t;

static int number_inodes(ext4_writer_t *w, vfs_node_t *root, numbering_t *n) {
    uint64_t max_inodes = (uint64_t)w->groups * INODES_PER_GROUP;
    size_t cap = 1024;
    n->by_ino = calloc(cap, sizeof(*n->by_ino));
    n->parent = calloc(cap, sizeof(*n->parent));
    if (!n->by_ino || !n->parent) {
        return -ENOMEM;
    }
    uint32_t next = FIRST_INO;
    root->ino = ROOT_INO;
    n->by_ino[ROOT_INO] = root;
    n->parent[ROOT_INO] = ROOT_INO;
    if (w->lost_found) {
        next++;
    }
    uint32_t queue_head = 0;
    uint32_t *queue = malloc(cap * sizeof(*queue));
    if (!queue) {
        return -ENOMEM;
    }
    size_t queue_cap = cap;
    uint32_t queue_len = 0;
    queue[queue_len++] = ROOT_INO;
    int rc = 0;
    while (rc == 0 && queue_head < queue_len) {
        uint32_t dir_ino = queue[queue_head++];
        vfs_node_t *dir = n->by_ino[dir_ino];
        dir->nlink = 2;
        for (size_t i = 0; i < dir->child_count && rc == 0; i++) {
            vfs_node_t *child = dir->children[i].node;
            if (S_ISDIR(child->mode)) {
                if (child->ino != 0 && n->parent[child->ino] != dir_ino) {
                    continue;
                }
                dir->nlink++;
            } else {
                child->nlink++;
            }
            if (child->ino != 0) {
                continue;
            }
            if (next > max_inodes) {
                rc = -ENOSPC;
                break;
            }
            uint32_t ino = child == w->lost_found ? FIRST_INO : next++;
            if (ino >= cap) {
                size_t grown = cap * 2;
                vfs_node_t **by_ino = realloc(n->by_ino, grown * sizeof(*by_ino));
                if (by_ino) {
                    n->by_ino = by_ino;
                }
                uint32_t *parent = realloc(n->parent, grown * sizeof(*parent));
                if (parent) {
                    n->parent = parent;
                }
                if (!by_ino || !parent) {
                    rc = -ENOMEM;
                    break;
                }
                memset(n->by_ino + cap, 0, (grown - cap) * sizeof(*n->by_ino));
                memset(n->parent + cap, 0, (grown - cap) * sizeof(*n->parent));
                cap = grown;
            }
            child->ino = ino;
            n->by_ino[ino] = child;
            n->parent[ino] = dir_ino;
            if (S_ISDIR(child->mode)) {
                if (queue_len == queue_cap) {
                    uint32_t *grown = realloc(queue, queue_cap * 2 * sizeof(*queue));
                    if (!grown) {
                        rc = -ENOMEM;
                        break;
                    }
                    queue = grown;
                    queue_cap *= 2;
                }
                queue[queue_len++] = ino;
            }
        }
    }
    free(queue);
    n->last_ino = next - 1;
    return rc;
}

static int free_unreachable(ext4_writer_t *w) {
    for (size_t i = 0; i < w->file_count; i++) {
        vfs_node_t *file = w->files[i];
        if (file->ino != 0) {
            continue;
        }
        for (size_t e = 0; e < file->extent_count; e++) {
            if (w->freed_count == w->freed_cap) {
                size_t cap = w->freed_cap ? w->freed_cap * 2 : 16;
                block_range_t *freed = realloc(w->freed, cap * sizeof(*freed));
                if (!freed) {
                    return -ENOMEM;
                }
                w->freed = freed;
                w->freed_cap = cap;
            }
            w->freed[w->freed_count++] = (block_range_t){
                .start = file->extents[e].physical,
                .end = file->extents[e].physical + file->extents[e].length,
            };
        }
    }
    return 0;
}

static void put_extent_header(uint8_t *p, uint16_t entries, uint16_t max, uint16_t depth) {
    put_le16(p + 0, EXT4_EXTENT_MAGIC);
    put_le16(p + 2, entries);
    put_le16(p + 4, max);
    put_le16(p + 6, depth);
    put_le32(p + 8, 0);
}

static void put_extent_leaf(uint8_t *p, const vfs_extent_t *e) {
    put_le32(p + 0, (uint32_t)e->logical);
    put_le16(p + 4, (uint16_t)e->length);
    put_le16(p + 6, (uint16_t)(e->physical >> 32));
    put_le32(p + 8, (uint32_t)e->physical);
}

static void put_extent_index(uint8_t *p, uint32_t logical, uint64_t block) {
    put_le32(p + 0, logical);
    put_le32(p + 4, (uint32_t)block);
    put_le16(p + 8, (uint16_t)(block >> 32));
    put_le16(p + 10, 0);
}

typedef struct extent_index {
    uint32_t logical;
    uint64_t block;
} extent_index_t;

static int build_extent_tree(ext4_writer_t *w, const vfs_extent_t *extents, size_t count, uint8_t iblock[60],
                             uint64_t *tree_blocks_out) {
    *tree_blocks_out = 0;
    memset(iblock, 0, 60);
    if (count <= EXTENTS_IN_INODE) {
        put_extent_header(iblock, (uint16_t)count, EXTENTS_IN_INODE, 0);
        for (size_t i = 0; i < count; i++) {
            put_extent_leaf(iblock + 12 + i * 12, &extents[i]);
        }
        return 0;
    }
    uint8_t *block = malloc(BLOCK_SIZE);
    size_t level_count = (count + EXTENTS_PER_BLOCK - 1) / EXTENTS_PER_BLOCK;
    extent_index_t *level = malloc(level_count * sizeof(*level));
    if (!block || !level) {
        free(block);
        free(level);
        return -ENOMEM;
    }
    int rc = 0;
    for (size_t leaf = 0; leaf < level_count && rc == 0; leaf++) {
        size_t first = leaf * EXTENTS_PER_BLOCK;
        size_t n = count - first < EXTENTS_PER_BLOCK ? count - first : EXTENTS_PER_BLOCK;
        memset(block, 0, BLOCK_SIZE);
        put_extent_header(block, (uint16_t)n, EXTENTS_PER_BLOCK, 0);
        for (size_t i = 0; i < n; i++) {
            put_extent_leaf(block + 12 + i * 12, &extents[first + i]);
        }
        uint64_t phys = 0;
        uint32_t got = 0;
        rc = alloc_blocks(w, 1, &phys, &got);
        if (rc == 0) {
            rc = write_blocks(w, phys, block, BLOCK_SIZE);
        }
        level[leaf] = (extent_index_t){.logical = (uint32_t)extents[first].logical, .block = phys};
        (*tree_blocks_out)++;
    }
    uint16_t depth = 1;
    while (rc == 0 && level_count > EXTENTS_IN_INODE) {
        size_t parents = (level_count + EXTENTS_PER_BLOCK - 1) / EXTENTS_PER_BLOCK;
        for (size_t p = 0; p < parents && rc == 0; p++) {
            size_t first = p * EXTENTS_PER_BLOCK;
            size_t n = level_count - first < EXTENTS_PER_BLOCK ? level_count - first : EXTENTS_PER_BLOCK;
            memset(block, 0, BLOCK_SIZE);
            put_extent_header(block, (uint16_t)n, EXTENTS_PER_BLOCK, depth);
            for (size_t i = 0; i < n; i++) {
                put_extent_index(block + 12 + i * 12, level[first + i].logical, level[first + i].block);
            }
            uint64_t phys = 0;
            uint32_t got = 0;
            rc = alloc_blocks(w, 1, &phys, &got);
            if (rc == 0) {
                rc = write_blocks(w, phys, block, BLOCK_SIZE);
            }
            level[p] = (extent_index_t){.logical = level[first].logical, .block = phys};
            (*tree_blocks_out)++;
        }
        level_count = parents;
        depth++;
    }
    if (rc == 0) {
        put_extent_header(iblock, (uint16_t)level_count, EXTENTS_IN_INODE, depth);
        for (size_t i = 0; i < level_count; i++) {
            put_extent_index(iblock + 12 + i * 12, level[i].logical, level[i].block);
        }
    }
    free(level);
    free(block);
    return rc;
}

typedef struct dir_builder {
    uint8_t *data;
    size_t cap;
    size_t len;
    size_t block_start;
    size_t last_entry;
} dir_builder_t;

static void dir_close_block(dir_builder_t *d) {
    if (d->len > d->block_start) {
        put_le16(d->data + d->last_entry + 4, (uint16_t)(d->block_start + BLOCK_SIZE - d->last_entry));
        d->block_start += BLOCK_SIZE;
        d->len = d->block_start;
    }
}

static int dir_reserve_block(dir_builder_t *d) {
    if (d->block_start + BLOCK_SIZE <= d->cap) {
        return 0;
    }
    size_t cap = d->cap ? d->cap * 2 : BLOCK_SIZE;
    uint8_t *data = realloc(d->data, cap);
    if (!data) {
        return -ENOMEM;
    }
    memset(data + d->cap, 0, cap - d->cap);
    d->data = data;
    d->cap = cap;
    return 0;
}

static int dir_add(dir_builder_t *d, uint32_t ino, const char *name, size_t name_len, uint8_t type) {
    size_t rec_len = align4(8 + name_len);
    if (d->len + rec_len > d->block_start + BLOCK_SIZE) {
        dir_close_block(d);
    }
    int rc = dir_reserve_block(d);
    if (rc < 0) {
        return rc;
    }
    uint8_t *p = d->data + d->len;
    put_le32(p, ino);
    put_le16(p + 4, (uint16_t)rec_len);
    p[6] = (uint8_t)name_len;
    p[7] = type;
    memcpy(p + 8, name, name_len);
    d->last_entry = d->len;
    d->len += rec_len;
    return 0;
}

static int dir_add_empty_block(dir_builder_t *d) {
    int rc = dir_reserve_block(d);
    if (rc < 0) {
        return rc;
    }
    uint8_t *p = d->data + d->block_start;
    put_le32(p, 0);
    put_le16(p + 4, BLOCK_SIZE);
    d->block_start += BLOCK_SIZE;
    d->len = d->block_start;
    return 0;
}

static int build_directory(ext4_writer_t *w, const numbering_t *n, vfs_node_t *dir, uint8_t **data_out,
                           size_t *blocks_out) {
    dir_builder_t d = {0};
    uint32_t parent = n->parent[dir->ino];
    int rc = dir_add(&d, dir->ino, ".", 1, FT_DIR);
    if (rc == 0) {
        rc = dir_add(&d, parent, "..", 2, FT_DIR);
    }
    for (size_t i = 0; i < dir->child_count && rc == 0; i++) {
        const vfs_dirent_t *de = &dir->children[i];
        if (S_ISDIR(de->node->mode) && n->parent[de->node->ino] != dir->ino) {
            continue;
        }
        rc = dir_add(&d, de->node->ino, de->name, de->name_len, file_type(de->node->mode));
    }
    if (rc == 0) {
        dir_close_block(&d);
        while (rc == 0 && dir == w->lost_found && d.block_start < (size_t)LOST_FOUND_BLOCKS * BLOCK_SIZE) {
            rc = dir_add_empty_block(&d);
        }
    }
    if (rc < 0) {
        free(d.data);
        return rc;
    }
    *data_out = d.data;
    *blocks_out = d.block_start / BLOCK_SIZE;
    return 0;
}

static uint32_t xattr_entry_hash(const uint8_t *name, size_t name_len, const uint8_t *value, size_t value_len) {
    uint32_t hash = 0;
    for (size_t i = 0; i < name_len; i++) {
        hash = (hash << 5) ^ (hash >> 27) ^ name[i];
    }
    for (size_t off = 0; off < value_len; off += 4) {
        uint8_t word[4] = {0};
        memcpy(word, value + off, value_len - off < 4 ? value_len - off : 4);
        hash = (hash << 16) ^ (hash >> 16) ^ get_le32(word);
    }
    return hash;
}

static int compare_xattrs(const void *a, const void *b) {
    const vfs_xattr_t *x = *(const vfs_xattr_t *const *)a;
    const vfs_xattr_t *y = *(const vfs_xattr_t *const *)b;
    if (x->index != y->index) {
        return x->index < y->index ? -1 : 1;
    }
    if (x->name_len != y->name_len) {
        return x->name_len < y->name_len ? -1 : 1;
    }
    return memcmp(x->payload, y->payload, x->name_len);
}

static size_t xattr_footprint(const vfs_xattr_t *const *xattrs, size_t count) {
    size_t total = 4;
    for (size_t i = 0; i < count; i++) {
        total += align4(XATTR_ENTRY_HEADER + xattrs[i]->name_len) + align4(xattrs[i]->value_len);
    }
    return total;
}

static uint32_t xattr_emit(uint8_t *base, size_t entries_off, size_t end, const vfs_xattr_t *const *xattrs,
                           size_t count) {
    size_t entry = entries_off;
    size_t value_end = end;
    uint32_t block_hash = 0;
    bool block_hash_valid = true;
    for (size_t i = 0; i < count; i++) {
        const vfs_xattr_t *x = xattrs[i];
        const uint8_t *value = x->payload + x->name_len;
        uint16_t value_offs = 0;
        if (x->value_len > 0) {
            value_end -= align4(x->value_len);
            memcpy(base + value_end, value, x->value_len);
            value_offs = (uint16_t)value_end;
        }
        uint32_t hash = xattr_entry_hash(x->payload, x->name_len, value, x->value_len);
        uint8_t *p = base + entry;
        p[0] = x->name_len;
        p[1] = x->index;
        put_le16(p + 2, value_offs);
        put_le32(p + 4, 0);
        put_le32(p + 8, x->value_len);
        put_le32(p + 12, hash);
        memcpy(p + XATTR_ENTRY_HEADER, x->payload, x->name_len);
        entry += align4(XATTR_ENTRY_HEADER + x->name_len);
        if (hash == 0) {
            block_hash_valid = false;
        }
        block_hash = (block_hash << 16) ^ (block_hash >> 16) ^ hash;
    }
    return block_hash_valid ? block_hash : 0;
}

static int place_xattrs(ext4_writer_t *w, const vfs_node_t *node, uint8_t *raw, uint64_t *xattr_block_out) {
    *xattr_block_out = 0;
    size_t count = 0;
    for (const vfs_xattr_t *x = node->xattrs; x; x = x->next) {
        count++;
    }
    if (count == 0) {
        return 0;
    }
    const vfs_xattr_t **list = malloc(count * sizeof(*list));
    if (!list) {
        return -ENOMEM;
    }
    size_t i = 0;
    for (const vfs_xattr_t *x = node->xattrs; x; x = x->next) {
        list[i++] = x;
    }
    qsort(list, count, sizeof(*list), compare_xattrs);
    int rc = 0;
    if (xattr_footprint(list, count) <= XATTR_IBODY_SPACE) {
        put_le32(raw + XATTR_IBODY_OFFSET, XATTR_MAGIC);
        xattr_emit(raw + XATTR_IBODY_OFFSET + 4, 0, XATTR_IBODY_SPACE, list, count);
    } else {
        while (count > 0 && XATTR_BLOCK_HEADER + xattr_footprint(list, count) > BLOCK_SIZE) {
            size_t largest = 0;
            for (size_t k = 1; k < count; k++) {
                if (list[k]->value_len > list[largest]->value_len) {
                    largest = k;
                }
            }
            memmove(list + largest, list + largest + 1, (count - largest - 1) * sizeof(*list));
            count--;
        }
        uint8_t *block = calloc(1, BLOCK_SIZE);
        uint64_t phys = 0;
        uint32_t got = 0;
        rc = block ? alloc_blocks(w, 1, &phys, &got) : -ENOMEM;
        if (rc == 0) {
            put_le32(block + 0, XATTR_MAGIC);
            put_le32(block + 4, 1);
            put_le32(block + 8, 1);
            put_le32(block + 12, xattr_emit(block, XATTR_BLOCK_HEADER, BLOCK_SIZE, list, count));
            rc = write_blocks(w, phys, block, BLOCK_SIZE);
        }
        free(block);
        if (rc == 0) {
            *xattr_block_out = phys;
        }
    }
    free(list);
    return rc;
}

static void put_time(uint8_t *raw, size_t base, size_t extra, int64_t sec, uint32_t nsec) {
    put_le32(raw + base, (uint32_t)sec);
    uint32_t epoch = (uint32_t)(((uint64_t)(sec - (int64_t)(int32_t)(uint32_t)sec)) >> 32) & 3U;
    put_le32(raw + extra, epoch | (nsec << 2));
}

typedef struct inode_shape {
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t links;
    uint32_t flags;
    uint64_t size;
    uint64_t blocks;
    int64_t mtime;
    uint32_t mtime_nsec;
    uint64_t xattr_block;
} inode_shape_t;

static void fill_inode(ext4_writer_t *w, uint8_t *raw, const inode_shape_t *s) {
    uint64_t sectors = s->blocks * (BLOCK_SIZE / 512);
    put_le16(raw + 0x00, (uint16_t)s->mode);
    put_le16(raw + 0x02, (uint16_t)s->uid);
    put_le32(raw + 0x04, (uint32_t)s->size);
    put_le16(raw + 0x18, (uint16_t)s->gid);
    put_le16(raw + 0x1A, (uint16_t)s->links);
    put_le32(raw + 0x1C, (uint32_t)sectors);
    put_le32(raw + 0x20, s->flags);
    put_le32(raw + 0x64, arc4random());
    put_le32(raw + 0x68, (uint32_t)s->xattr_block);
    put_le32(raw + 0x6C, (uint32_t)(s->size >> 32));
    put_le16(raw + 0x74, (uint16_t)(sectors >> 32));
    put_le16(raw + 0x76, (uint16_t)(s->xattr_block >> 32));
    put_le16(raw + 0x78, (uint16_t)(s->uid >> 16));
    put_le16(raw + 0x7A, (uint16_t)(s->gid >> 16));
    put_le16(raw + 0x80, INODE_EXTRA_ISIZE);
    put_time(raw, 0x08, 0x8C, s->mtime, s->mtime_nsec);
    put_time(raw, 0x0C, 0x84, w->now, 0);
    put_time(raw, 0x10, 0x88, s->mtime, s->mtime_nsec);
    put_time(raw, 0x90, 0x94, w->now, 0);
}

static int write_extent_file(ext4_writer_t *w, vfs_node_t *node, uint8_t *raw, inode_shape_t *shape) {
    uint64_t tree_blocks = 0;
    int rc = build_extent_tree(w, node->extents, node->extent_count, raw + 0x28, &tree_blocks);
    if (rc < 0) {
        return rc;
    }
    uint64_t data_blocks = 0;
    for (size_t i = 0; i < node->extent_count; i++) {
        data_blocks += node->extents[i].length;
    }
    shape->flags |= EXT4_EXTENTS_FL;
    shape->blocks += data_blocks + tree_blocks;
    return 0;
}

static int build_node_inode(ext4_writer_t *w, const numbering_t *n, vfs_node_t *node, uint8_t *raw,
                            uint32_t *dirs) {
    inode_shape_t shape = {
        .mode = node->mode,
        .uid = node->uid,
        .gid = node->gid,
        .links = node->nlink,
        .mtime = node->mtime,
        .mtime_nsec = node->mtime_nsec,
    };
    int rc = 0;
    if (S_ISDIR(node->mode)) {
        uint8_t *data = NULL;
        size_t blocks = 0;
        rc = build_directory(w, n, node, &data, &blocks);
        if (rc == 0) {
            rc = write_extent_data(w, node, 0, data, blocks);
        }
        free(data);
        shape.size = (uint64_t)blocks * BLOCK_SIZE;
        if (shape.links >= LINK_MAX) {
            shape.links = 1;
        }
        (*dirs)++;
        if (rc == 0) {
            rc = write_extent_file(w, node, raw, &shape);
        }
    } else if (S_ISREG(node->mode)) {
        if (node->nlink >= LINK_MAX) {
            return -EMLINK;
        }
        shape.size = node->size;
        rc = write_extent_file(w, node, raw, &shape);
    } else if (S_ISLNK(node->mode)) {
        size_t len = node->symlink ? strlen(node->symlink) : 0;
        if (len == 0 || len >= BLOCK_SIZE) {
            return -ENAMETOOLONG;
        }
        shape.size = len;
        if (len < FAST_SYMLINK_MAX) {
            memcpy(raw + 0x28, node->symlink, len);
        } else {
            uint8_t *block = calloc(1, BLOCK_SIZE);
            if (!block) {
                return -ENOMEM;
            }
            memcpy(block, node->symlink, len);
            rc = write_extent_data(w, node, 0, block, 1);
            free(block);
            if (rc == 0) {
                rc = write_extent_file(w, node, raw, &shape);
            }
        }
    } else if (S_ISCHR(node->mode) || S_ISBLK(node->mode)) {
        uint32_t major = node->rdev_major;
        uint32_t minor = node->rdev_minor;
        if (major < 256 && minor < 256) {
            put_le32(raw + 0x28, (major << 8) | minor);
        } else {
            put_le32(raw + 0x2C, (minor & 0xFFU) | ((major & 0xFFFU) << 8) | ((minor & ~0xFFU) << 12));
        }
    }
    if (rc == 0) {
        rc = place_xattrs(w, node, raw, &shape.xattr_block);
    }
    if (rc < 0) {
        return rc;
    }
    if (shape.xattr_block != 0) {
        shape.blocks++;
    }
    fill_inode(w, raw, &shape);
    return 0;
}

static void build_journal_inode(ext4_writer_t *w, uint8_t *raw) {
    vfs_extent_t extent = {.logical = 0, .physical = w->journal_start, .length = w->journal_blocks};
    put_extent_header(raw + 0x28, 1, EXTENTS_IN_INODE, 0);
    put_extent_leaf(raw + 0x28 + 12, &extent);
    inode_shape_t shape = {
        .mode = S_IFREG | 0600,
        .links = 1,
        .flags = EXT4_EXTENTS_FL,
        .size = (uint64_t)w->journal_blocks * BLOCK_SIZE,
        .blocks = w->journal_blocks,
        .mtime = w->now,
    };
    fill_inode(w, raw, &shape);
}

static int write_inode_tables(ext4_writer_t *w, const numbering_t *n) {
    uint8_t *table = malloc((size_t)INODES_PER_GROUP * INODE_SIZE);
    if (!table) {
        return -ENOMEM;
    }
    int rc = 0;
    uint32_t used_groups = (n->last_ino + INODES_PER_GROUP - 1) / INODES_PER_GROUP;
    for (uint32_t g = 0; g < used_groups && rc == 0; g++) {
        memset(table, 0, (size_t)INODES_PER_GROUP * INODE_SIZE);
        uint32_t first = g * INODES_PER_GROUP + 1;
        uint32_t last = first + INODES_PER_GROUP - 1;
        if (last > n->last_ino) {
            last = n->last_ino;
        }
        uint32_t dirs = 0;
        for (uint32_t ino = first; ino <= last && rc == 0; ino++) {
            uint8_t *raw = table + (size_t)(ino - first) * INODE_SIZE;
            if (ino == JOURNAL_INO) {
                build_journal_inode(w, raw);
            } else if (n->by_ino[ino]) {
                rc = build_node_inode(w, n, n->by_ino[ino], raw, &dirs);
            }
        }
        w->layout[g].dirs = dirs;
        size_t bytes = ((size_t)(last - first + 1) * INODE_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE;
        if (rc == 0) {
            rc = write_blocks(w, w->layout[g].inode_table, table, bytes);
        }
    }
    free(table);
    return rc;
}

static void set_bits(uint8_t *bitmap, uint64_t from, uint64_t to) {
    for (uint64_t bit = from; bit < to; bit++) {
        bitmap[bit >> 3] |= (uint8_t)(1U << (bit & 7));
    }
}

static void clear_bits(uint8_t *bitmap, uint64_t from, uint64_t to) {
    for (uint64_t bit = from; bit < to; bit++) {
        bitmap[bit >> 3] &= (uint8_t)~(1U << (bit & 7));
    }
}

static uint32_t count_bits(const uint8_t *bitmap, uint64_t bits) {
    uint32_t count = 0;
    for (uint64_t i = 0; i < bits / 8; i++) {
        count += (uint32_t)__builtin_popcount(bitmap[i]);
    }
    for (uint64_t bit = bits & ~(uint64_t)7; bit < bits; bit++) {
        count += (bitmap[bit >> 3] >> (bit & 7)) & 1U;
    }
    return count;
}

static void clip_range(uint64_t start, uint64_t end, uint64_t group_start, uint64_t group_end,
                       uint64_t *from, uint64_t *to) {
    *from = start > group_start ? start : group_start;
    *to = end < group_end ? end : group_end;
}

static int write_bitmaps(ext4_writer_t *w, const numbering_t *n) {
    uint8_t *bitmap = malloc(BLOCK_SIZE);
    if (!bitmap) {
        return -ENOMEM;
    }
    int rc = 0;
    for (uint32_t g = 0; g < w->groups && rc == 0; g++) {
        uint64_t start = group_first_block(g);
        uint64_t size = group_block_count(w, g);
        uint64_t end = start + size;
        uint64_t from;
        uint64_t to;

        memset(bitmap, 0, BLOCK_SIZE);
        clip_range(0, w->cursor, start, end, &from, &to);
        if (from < to) {
            set_bits(bitmap, from - start, to - start);
        }
        for (size_t i = 0; i < w->reserved_count; i++) {
            clip_range(w->reserved[i].start, w->reserved[i].end, start, end, &from, &to);
            if (from < to) {
                set_bits(bitmap, from - start, to - start);
            }
        }
        for (size_t i = 0; i < w->freed_count; i++) {
            clip_range(w->freed[i].start, w->freed[i].end, start, end, &from, &to);
            if (from < to) {
                clear_bits(bitmap, from - start, to - start);
            }
        }
        w->layout[g].free_blocks = (uint32_t)(size - count_bits(bitmap, size));
        set_bits(bitmap, size, (uint64_t)BLOCK_SIZE * 8);
        rc = write_blocks(w, w->layout[g].block_bitmap, bitmap, BLOCK_SIZE);
        if (rc < 0) {
            break;
        }

        memset(bitmap, 0, BLOCK_SIZE);
        uint64_t first_ino = (uint64_t)g * INODES_PER_GROUP + 1;
        uint64_t used = n->last_ino >= first_ino ? n->last_ino - first_ino + 1 : 0;
        if (used > INODES_PER_GROUP) {
            used = INODES_PER_GROUP;
        }
        set_bits(bitmap, 0, used);
        set_bits(bitmap, INODES_PER_GROUP, (uint64_t)BLOCK_SIZE * 8);
        w->layout[g].free_inodes = INODES_PER_GROUP - (uint32_t)used;
        rc = write_blocks(w, w->layout[g].inode_bitmap, bitmap, BLOCK_SIZE);
    }
    free(bitmap);
    return rc;
}

static void build_superblock(ext4_writer_t *w, uint8_t *sb, uint32_t group, uint64_t free_blocks,
                             uint64_t free_inodes, const uint8_t journal_iblock[60]) {
    memset(sb, 0, 1024);
    uint64_t inodes = (uint64_t)w->groups * INODES_PER_GROUP;
    put_le32(sb + 0x00, (uint32_t)inodes);
    put_le32(sb + 0x04, (uint32_t)w->total_blocks);
    put_le32(sb + 0x0C, (uint32_t)free_blocks);
    put_le32(sb + 0x10, (uint32_t)free_inodes);
    put_le32(sb + 0x14, 0);
    put_le32(sb + 0x18, 2);
    put_le32(sb + 0x1C, 2);
    put_le32(sb + 0x20, BLOCKS_PER_GROUP);
    put_le32(sb + 0x24, BLOCKS_PER_GROUP);
    put_le32(sb + 0x28, INODES_PER_GROUP);
    put_le32(sb + 0x30, (uint32_t)w->now);
    put_le16(sb + 0x36, 0xFFFF);
    put_le16(sb + 0x38, EXT4_SUPER_MAGIC);
    put_le16(sb + 0x3A, 1);
    put_le16(sb + 0x3C, 1);
    put_le32(sb + 0x40, (uint32_t)w->now);
    put_le32(sb + 0x4C, 1);
    put_le32(sb + 0x54, FIRST_INO);
    put_le16(sb + 0x58, INODE_SIZE);
    put_le16(sb + 0x5A, (uint16_t)group);
    put_le32(sb + 0x5C, COMPAT_HAS_JOURNAL | COMPAT_EXT_ATTR | COMPAT_DIR_INDEX);
    put_le32(sb + 0x60, INCOMPAT_FILETYPE | INCOMPAT_EXTENTS | INCOMPAT_64BIT | INCOMPAT_FLEX_BG);
    put_le32(sb + 0x64, RO_COMPAT_SPARSE_SUPER | RO_COMPAT_LARGE_FILE | RO_COMPAT_HUGE_FILE |
                            RO_COMPAT_DIR_NLINK | RO_COMPAT_EXTRA_ISIZE);
    memcpy(sb + 0x68, w->uuid, 16);
    put_le32(sb + 0xE0, JOURNAL_INO);
    memcpy(sb + 0xEC, w->hash_seed, 16);
    sb[0xFC] = HASH_HALF_MD4;
    sb[0xFD] = JNL_BACKUP_BLOCKS;
    put_le16(sb + 0xFE, DESC_SIZE);
    put_le32(sb + 0x100, DEFM_XATTR_USER | DEFM_ACL);
    put_le32(sb + 0x108, (uint32_t)w->now);
    memcpy(sb + 0x10C, journal_iblock, 60);
    uint64_t journal_bytes = (uint64_t)w->journal_blocks * BLOCK_SIZE;
    put_le32(sb + 0x10C + 60, (uint32_t)(journal_bytes >> 32));
    put_le32(sb + 0x10C + 64, (uint32_t)journal_bytes);
    put_le32(sb + 0x150, (uint32_t)(w->total_blocks >> 32));
    put_le32(sb + 0x158, (uint32_t)(free_blocks >> 32));
    put_le16(sb + 0x15C, INODE_EXTRA_ISIZE);
    put_le16(sb + 0x15E, INODE_EXTRA_ISIZE);
    put_le32(sb + 0x160, FLAGS_UNSIGNED_HASH);
    sb[0x174] = LOG_GROUPS_PER_FLEX;
}

static int write_descriptors_and_supers(ext4_writer_t *w) {
    size_t gdt_bytes = (size_t)w->gdt_blocks * BLOCK_SIZE;
    uint8_t *gdt = calloc(1, gdt_bytes);
    uint8_t *super_block = calloc(1, BLOCK_SIZE);
    if (!gdt || !super_block) {
        free(gdt);
        free(super_block);
        return -ENOMEM;
    }
    uint64_t free_blocks = 0;
    uint64_t free_inodes = 0;
    for (uint32_t g = 0; g < w->groups; g++) {
        const group_layout_t *l = &w->layout[g];
        uint8_t *d = gdt + (size_t)g * DESC_SIZE;
        put_le32(d + 0x00, (uint32_t)l->block_bitmap);
        put_le32(d + 0x04, (uint32_t)l->inode_bitmap);
        put_le32(d + 0x08, (uint32_t)l->inode_table);
        put_le16(d + 0x0C, (uint16_t)l->free_blocks);
        put_le16(d + 0x0E, (uint16_t)l->free_inodes);
        put_le16(d + 0x10, (uint16_t)l->dirs);
        put_le32(d + 0x20, (uint32_t)(l->block_bitmap >> 32));
        put_le32(d + 0x24, (uint32_t)(l->inode_bitmap >> 32));
        put_le32(d + 0x28, (uint32_t)(l->inode_table >> 32));
        put_le16(d + 0x2C, (uint16_t)(l->free_blocks >> 16));
        put_le16(d + 0x2E, (uint16_t)(l->free_inodes >> 16));
        put_le16(d + 0x30, (uint16_t)(l->dirs >> 16));
        free_blocks += l->free_blocks;
        free_inodes += l->free_inodes;
    }

    uint8_t journal_iblock[60] = {0};
    vfs_extent_t extent = {.logical = 0, .physical = w->journal_start, .length = w->journal_blocks};
    put_extent_header(journal_iblock, 1, EXTENTS_IN_INODE, 0);
    put_extent_leaf(journal_iblock + 12, &extent);

    int rc = 0;
    for (uint32_t g = 0; g < w->groups && rc == 0; g++) {
        if (!group_has_super(g)) {
            continue;
        }
        memset(super_block, 0, BLOCK_SIZE);
        build_superblock(w, super_block + (g == 0 ? 1024 : 0), g, free_blocks, free_inodes, journal_iblock);
        rc = write_blocks(w, group_first_block(g), super_block, BLOCK_SIZE);
        if (rc == 0) {
            rc = write_blocks(w, group_first_block(g) + 1, gdt, gdt_bytes);
        }
    }
    free(gdt);
    free(super_block);
    return rc;
}

static int write_journal_superblock(ext4_writer_t *w) {
    uint8_t *block = calloc(1, BLOCK_SIZE);
    if (!block) {
        return -ENOMEM;
    }
    put_be32(block + 0x00, JBD2_MAGIC);
    put_be32(block + 0x04, JBD2_SUPERBLOCK_V2);
    put_be32(block + 0x0C, BLOCK_SIZE);
    put_be32(block + 0x10, w->journal_blocks);
    put_be32(block + 0x14, 1);
    put_be32(block + 0x18, 1);
    memcpy(block + 0x30, w->uuid, 16);
    put_be32(block + 0x40, 1);
    int rc = write_blocks(w, w->journal_start, block, BLOCK_SIZE);
    free(block);
    return rc;
}

static int prepare_lost_found(ext4_writer_t *w, vfs_node_t *root) {
    vfs_dirent_t *existing = vfs_dir_find(root, "lost+found", 10);
    if (existing && S_ISDIR(existing->node->mode)) {
        return 0;
    }
    vfs_node_t *dir = vfs_node_new(S_IFDIR | 0700);
    if (!dir) {
        return -ENOMEM;
    }
    dir->mtime = w->now;
    bool ok = vfs_dir_set(root, "lost+found", 10, dir);
    if (ok) {
        w->lost_found = dir;
    }
    vfs_node_release(dir);
    return ok ? 0 : -ENOMEM;
}

int ext4_writer_finish(ext4_writer_t *w, vfs_node_t *root) {
    if (w->file || !S_ISDIR(root->mode)) {
        return -EINVAL;
    }
    numbering_t n = {0};
    int rc = prepare_lost_found(w, root);
    if (rc == 0) {
        rc = number_inodes(w, root, &n);
    }
    if (rc == 0) {
        rc = free_unreachable(w);
    }
    if (rc == 0) {
        rc = write_inode_tables(w, &n);
    }
    if (rc == 0) {
        rc = write_bitmaps(w, &n);
    }
    if (rc == 0) {
        rc = write_journal_superblock(w);
    }
    if (rc == 0) {
        rc = write_descriptors_and_supers(w);
    }
    free(n.by_ino);
    free(n.parent);
    return rc;
}
