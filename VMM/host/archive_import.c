#define _DARWIN_C_SOURCE

#include "host_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <zstd.h>

enum : uint32_t {
    STREAM_IO_BUF_SIZE = 65536U,
    XZ_MT_BUF_SIZE = 1U << 20,
    XZ_TAIL_SIZE = 4096U,
    XZ_INDEX_MAX_SIZE = 4U << 20,
    VFS_MAX_DEPTH = 128U,
    VFS_MAX_SYMLINKS = 40U,
    VFS_MAX_NAME_LEN = 255U,
    VFS_MAX_SYMLINK_LEN = 4095U,
    VFS_MAX_XATTR_VALUE = 4096U,
    VFS_HASH_THRESHOLD = 32U,
    VFS_MAX_ENTRIES = 4000000U,
};

enum : uint8_t {
    XATTR_INDEX_USER = 1,
    XATTR_INDEX_POSIX_ACL_ACCESS = 2,
    XATTR_INDEX_POSIX_ACL_DEFAULT = 3,
    XATTR_INDEX_TRUSTED = 4,
    XATTR_INDEX_SECURITY = 6,
};

enum : uint16_t {
    ACL_USER_OBJ = 0x01,
    ACL_USER = 0x02,
    ACL_GROUP_OBJ = 0x04,
    ACL_GROUP = 0x08,
    ACL_MASK = 0x10,
    ACL_OTHER = 0x20,
};

static const uint64_t DEFAULT_DISK_SIZE = 64ULL << 30;

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

static inline uint16_t get_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t get_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void set_error(char **error_message, const char *msg) {
    if (error_message && !*error_message) {
        *error_message = strdup(msg ? msg : "unknown error");
    }
}

static void set_errno_error(char **error_message, const char *what, int err) {
    if (error_message && !*error_message) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s: %s", what, strerror(err));
        *error_message = strdup(buf);
    }
}

static uint32_t fnv1a_32(const char *s, size_t len) {
    uint32_t h = 2166136261U;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)s[i];
        h *= 16777619U;
    }
    return h;
}

vfs_node_t *vfs_node_new(uint32_t mode) {
    vfs_node_t *n = calloc(1, sizeof(*n));
    if (!n) {
        return NULL;
    }
    n->mode = mode;
    n->refcount = 1;
    n->mtime = (int64_t)time(NULL);
    return n;
}

vfs_node_t *vfs_node_retain(vfs_node_t *node) {
    if (node) {
        node->refcount++;
    }
    return node;
}

static void vfs_node_free_xattrs(vfs_node_t *n) {
    vfs_xattr_t *x = n->xattrs;
    while (x) {
        vfs_xattr_t *next = x->next;
        free(x);
        x = next;
    }
    n->xattrs = NULL;
}

static void vfs_node_destroy(vfs_node_t *n) {
    vfs_node_free_xattrs(n);
    free(n->children);
    free(n->hash_slots);
    free(n->symlink);
    free(n->extents);
    free(n);
}

void vfs_node_release(vfs_node_t *root) {
    if (!root || --root->refcount > 0) {
        return;
    }
    size_t cap = 64;
    size_t top = 0;
    vfs_node_t **stack = malloc(cap * sizeof(*stack));
    if (!stack) {
        for (size_t i = 0; i < root->child_count; i++) {
            free(root->children[i].name);
            vfs_node_release(root->children[i].node);
        }
        vfs_node_destroy(root);
        return;
    }
    stack[top++] = root;
    while (top > 0) {
        vfs_node_t *n = stack[--top];
        for (size_t i = 0; i < n->child_count; i++) {
            free(n->children[i].name);
            vfs_node_t *child = n->children[i].node;
            if (child && --child->refcount == 0) {
                if (top == cap) {
                    vfs_node_t **next = realloc(stack, cap * 2 * sizeof(*next));
                    if (!next) {
                        child->refcount = 1;
                        vfs_node_release(child);
                        continue;
                    }
                    stack = next;
                    cap *= 2;
                }
                stack[top++] = child;
            }
        }
        vfs_node_destroy(n);
    }
    free(stack);
}

static void vfs_dir_clear(vfs_node_t *dir) {
    for (size_t i = 0; i < dir->child_count; i++) {
        free(dir->children[i].name);
        vfs_node_release(dir->children[i].node);
    }
    dir->child_count = 0;
    if (dir->hash_slots) {
        memset(dir->hash_slots, 0, dir->hash_cap * sizeof(uint32_t));
    }
}

static bool vfs_dir_rehash(vfs_node_t *dir, size_t min_cap) {
    size_t new_cap = 64;
    while (new_cap < min_cap * 2) {
        new_cap *= 2;
    }
    uint32_t *slots = calloc(new_cap, sizeof(uint32_t));
    if (!slots) {
        return false;
    }
    size_t mask = new_cap - 1;
    for (size_t i = 0; i < dir->child_count; i++) {
        size_t pos = dir->children[i].hash & mask;
        while (slots[pos] != 0) {
            pos = (pos + 1) & mask;
        }
        slots[pos] = (uint32_t)(i + 1);
    }
    free(dir->hash_slots);
    dir->hash_slots = slots;
    dir->hash_cap = new_cap;
    return true;
}

static ssize_t vfs_dir_find_index(const vfs_node_t *dir, const char *name, size_t nlen, uint32_t hash) {
    if (dir->hash_slots && dir->hash_cap > 0) {
        size_t mask = dir->hash_cap - 1;
        size_t pos = hash & mask;
        while (dir->hash_slots[pos] != 0) {
            size_t idx = (size_t)(dir->hash_slots[pos] - 1);
            const vfs_dirent_t *de = &dir->children[idx];
            if (de->hash == hash && de->name_len == nlen && memcmp(de->name, name, nlen) == 0) {
                return (ssize_t)idx;
            }
            pos = (pos + 1) & mask;
        }
        return -1;
    }
    for (size_t i = 0; i < dir->child_count; i++) {
        const vfs_dirent_t *de = &dir->children[i];
        if (de->hash == hash && de->name_len == nlen && memcmp(de->name, name, nlen) == 0) {
            return (ssize_t)i;
        }
    }
    return -1;
}

vfs_dirent_t *vfs_dir_find(vfs_node_t *dir, const char *name, size_t name_len) {
    ssize_t idx = vfs_dir_find_index(dir, name, name_len, fnv1a_32(name, name_len));
    return idx >= 0 ? &dir->children[idx] : NULL;
}

static bool vfs_dir_remove(vfs_node_t *dir, const char *name, size_t nlen) {
    ssize_t idx = vfs_dir_find_index(dir, name, nlen, fnv1a_32(name, nlen));
    if (idx < 0) {
        return false;
    }
    size_t i = (size_t)idx;
    free(dir->children[i].name);
    vfs_node_release(dir->children[i].node);
    if (i + 1 < dir->child_count) {
        dir->children[i] = dir->children[dir->child_count - 1];
    }
    dir->child_count--;
    if (dir->hash_slots) {
        if (dir->child_count < VFS_HASH_THRESHOLD / 2) {
            free(dir->hash_slots);
            dir->hash_slots = NULL;
            dir->hash_cap = 0;
        } else {
            (void)vfs_dir_rehash(dir, dir->child_count + 1);
        }
    }
    return true;
}

bool vfs_dir_set(vfs_node_t *dir, const char *name, size_t nlen, vfs_node_t *child) {
    if (nlen == 0 || nlen > VFS_MAX_NAME_LEN) {
        return false;
    }
    uint32_t h = fnv1a_32(name, nlen);
    ssize_t idx = vfs_dir_find_index(dir, name, nlen, h);
    if (idx >= 0) {
        vfs_dirent_t *existing = &dir->children[idx];
        if (existing->node == child) {
            return true;
        }
        vfs_node_release(existing->node);
        existing->node = vfs_node_retain(child);
        return true;
    }
    if (dir->child_count == dir->child_cap) {
        size_t next_cap = dir->child_cap ? dir->child_cap * 2 : 8;
        vfs_dirent_t *next = realloc(dir->children, next_cap * sizeof(*next));
        if (!next) {
            return false;
        }
        dir->children = next;
        dir->child_cap = next_cap;
    }
    char *copy = malloc(nlen + 1);
    if (!copy) {
        return false;
    }
    memcpy(copy, name, nlen);
    copy[nlen] = '\0';
    size_t new_idx = dir->child_count++;
    dir->children[new_idx] = (vfs_dirent_t){
        .name = copy,
        .hash = h,
        .name_len = (uint16_t)nlen,
        .node = vfs_node_retain(child),
    };
    if (dir->hash_slots) {
        if (dir->child_count * 2 >= dir->hash_cap) {
            if (!vfs_dir_rehash(dir, dir->child_count + 1)) {
                free(dir->hash_slots);
                dir->hash_slots = NULL;
                dir->hash_cap = 0;
            }
        } else {
            size_t mask = dir->hash_cap - 1;
            size_t pos = h & mask;
            while (dir->hash_slots[pos] != 0) {
                pos = (pos + 1) & mask;
            }
            dir->hash_slots[pos] = (uint32_t)(new_idx + 1);
        }
    } else if (dir->child_count >= VFS_HASH_THRESHOLD) {
        (void)vfs_dir_rehash(dir, dir->child_count + 1);
    }
    return true;
}

typedef struct vfs_path_stack {
    vfs_node_t *nodes[VFS_MAX_DEPTH + 1];
    size_t depth;
} vfs_path_stack_t;

static bool vfs_walk_dir_path(vfs_node_t *root, vfs_path_stack_t *st, const char *path,
                              size_t path_len, bool create_dirs, unsigned *symlink_depth);

static bool vfs_follow_symlink_dir(vfs_node_t *root, vfs_path_stack_t *st,
                                   const char *target, bool create_dirs, unsigned *symlink_depth) {
    if (!target || target[0] == '\0' || *symlink_depth >= VFS_MAX_SYMLINKS) {
        return false;
    }
    (*symlink_depth)++;
    vfs_path_stack_t next_st = *st;
    if (target[0] == '/') {
        next_st.nodes[0] = root;
        next_st.depth = 0;
    }
    if (!vfs_walk_dir_path(root, &next_st, target, strlen(target), create_dirs, symlink_depth)) {
        return false;
    }
    *st = next_st;
    return true;
}

static vfs_dirent_t *vfs_make_implicit_dir(vfs_node_t *cur, const char *name, size_t len) {
    vfs_node_t *dir = vfs_node_new(S_IFDIR | 0755);
    if (!dir) {
        return NULL;
    }
    dir->implicit_dir = true;
    bool ok = vfs_dir_set(cur, name, len, dir);
    vfs_node_release(dir);
    return ok ? vfs_dir_find(cur, name, len) : NULL;
}

static bool vfs_enter(vfs_node_t *root, vfs_path_stack_t *st, vfs_dirent_t *de, bool create_dirs,
                      unsigned *symlink_depth) {
    if (S_ISLNK(de->node->mode)) {
        return vfs_follow_symlink_dir(root, st, de->node->symlink, create_dirs, symlink_depth);
    }
    if (!S_ISDIR(de->node->mode) || st->depth >= VFS_MAX_DEPTH) {
        return false;
    }
    st->nodes[++st->depth] = de->node;
    return true;
}

static bool vfs_walk_dir_path(vfs_node_t *root, vfs_path_stack_t *st, const char *path,
                              size_t path_len, bool create_dirs, unsigned *symlink_depth) {
    const char *p = path;
    const char *end = path + path_len;
    while (p < end) {
        while (p < end && *p == '/') {
            p++;
        }
        if (p >= end) {
            break;
        }
        const char *slash = memchr(p, '/', (size_t)(end - p));
        size_t clen = slash ? (size_t)(slash - p) : (size_t)(end - p);
        const char *next = slash ? slash + 1 : end;
        if (clen == 1 && p[0] == '.') {
            p = next;
            continue;
        }
        if (clen == 2 && p[0] == '.' && p[1] == '.') {
            if (st->depth > 0) {
                st->depth--;
            }
            p = next;
            continue;
        }
        if (clen > VFS_MAX_NAME_LEN) {
            return false;
        }
        vfs_node_t *cur = st->nodes[st->depth];
        if (!S_ISDIR(cur->mode)) {
            return false;
        }
        vfs_dirent_t *de = vfs_dir_find(cur, p, clen);
        if (!de) {
            if (!create_dirs || !(de = vfs_make_implicit_dir(cur, p, clen))) {
                return false;
            }
        }
        if (!vfs_enter(root, st, de, create_dirs, symlink_depth)) {
            return false;
        }
        p = next;
    }
    return true;
}

static bool vfs_resolve_parent(vfs_node_t *root, const char *raw_path, bool create_parents,
                               vfs_path_stack_t *parent_stack_out,
                               vfs_node_t **parent_out, const char **leaf_out, size_t *leaf_len_out) {
    const char *p = raw_path;
    while (*p == '/' || (p[0] == '.' && p[1] == '/')) {
        p += (*p == '/') ? 1 : 2;
    }
    size_t len = strlen(p);
    while (len > 0 && p[len - 1] == '/') {
        len--;
    }
    vfs_path_stack_t st = {.nodes = {root}, .depth = 0};
    *parent_out = NULL;
    *leaf_out = NULL;
    *leaf_len_out = 0;
    if (len == 0 || (len == 1 && p[0] == '.')) {
        if (parent_stack_out) {
            *parent_stack_out = st;
        }
        return true;
    }

    unsigned sym_depth = 0;
    const char *end = p + len;
    while (p < end) {
        while (p < end && *p == '/') {
            p++;
        }
        if (p >= end) {
            break;
        }
        const char *slash = memchr(p, '/', (size_t)(end - p));
        size_t clen = slash ? (size_t)(slash - p) : (size_t)(end - p);
        if (clen == 1 && p[0] == '.') {
            p = slash ? slash + 1 : end;
            continue;
        }
        if (clen == 2 && p[0] == '.' && p[1] == '.') {
            if (st.depth == 0) {
                return false;
            }
            st.depth--;
            p = slash ? slash + 1 : end;
            continue;
        }
        if (clen > VFS_MAX_NAME_LEN) {
            return false;
        }
        vfs_node_t *cur = st.nodes[st.depth];
        if (!S_ISDIR(cur->mode)) {
            return false;
        }
        if (!slash) {
            if (parent_stack_out) {
                *parent_stack_out = st;
            }
            *parent_out = cur;
            *leaf_out = p;
            *leaf_len_out = clen;
            return true;
        }
        vfs_dirent_t *de = vfs_dir_find(cur, p, clen);
        if (!de) {
            if (!create_parents || !(de = vfs_make_implicit_dir(cur, p, clen))) {
                return false;
            }
        }
        if (!vfs_enter(root, &st, de, create_parents, &sym_depth)) {
            return false;
        }
        p = slash + 1;
    }
    if (parent_stack_out) {
        *parent_stack_out = st;
    }
    return true;
}

static vfs_node_t *vfs_lookup(vfs_node_t *root, const char *raw_path) {
    vfs_node_t *parent = NULL;
    const char *leaf = NULL;
    size_t leaf_len = 0;
    if (!vfs_resolve_parent(root, raw_path, false, NULL, &parent, &leaf, &leaf_len)) {
        return NULL;
    }
    if (!parent) {
        return root;
    }
    vfs_dirent_t *de = vfs_dir_find(parent, leaf, leaf_len);
    return de ? de->node : NULL;
}

static bool vfs_dir_merge(vfs_node_t *dst, vfs_node_t *src, unsigned depth) {
    if (dst == src || depth > VFS_MAX_DEPTH) {
        return true;
    }
    for (size_t i = 0; i < src->child_count; i++) {
        const vfs_dirent_t *se = &src->children[i];
        vfs_dirent_t *de = vfs_dir_find(dst, se->name, se->name_len);
        if (!de) {
            if (!vfs_dir_set(dst, se->name, se->name_len, se->node)) {
                return false;
            }
        } else if (S_ISDIR(de->node->mode) && S_ISDIR(se->node->mode)) {
            if (!vfs_dir_merge(de->node, se->node, depth + 1)) {
                return false;
            }
        }
    }
    return true;
}

static uint8_t xattr_index_for(const char *name, const char **suffix_out) {
    static const struct {
        const char *prefix;
        size_t len;
        uint8_t index;
    } prefixes[] = {
        {"user.", 5, XATTR_INDEX_USER},
        {"trusted.", 8, XATTR_INDEX_TRUSTED},
        {"security.", 9, XATTR_INDEX_SECURITY},
    };
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        if (strncmp(name, prefixes[i].prefix, prefixes[i].len) == 0 && name[prefixes[i].len] != '\0') {
            *suffix_out = name + prefixes[i].len;
            return prefixes[i].index;
        }
    }
    *suffix_out = "";
    if (strcmp(name, "system.posix_acl_access") == 0) {
        return XATTR_INDEX_POSIX_ACL_ACCESS;
    }
    if (strcmp(name, "system.posix_acl_default") == 0) {
        return XATTR_INDEX_POSIX_ACL_DEFAULT;
    }
    return 0;
}

static bool node_set_xattr(vfs_node_t *node, uint8_t index, const char *suffix, size_t suffix_len,
                           const void *value, size_t value_len) {
    if (suffix_len > VFS_MAX_NAME_LEN || value_len > VFS_MAX_XATTR_VALUE) {
        return true;
    }
    for (vfs_xattr_t **pp = &node->xattrs; *pp; pp = &(*pp)->next) {
        vfs_xattr_t *x = *pp;
        if (x->index == index && x->name_len == suffix_len && memcmp(x->payload, suffix, suffix_len) == 0) {
            *pp = x->next;
            free(x);
            break;
        }
    }
    vfs_xattr_t *x = malloc(sizeof(*x) + suffix_len + value_len);
    if (!x) {
        return false;
    }
    x->index = index;
    x->name_len = (uint8_t)suffix_len;
    x->value_len = (uint16_t)value_len;
    memcpy(x->payload, suffix, suffix_len);
    if (value_len > 0) {
        memcpy(x->payload + suffix_len, value, value_len);
    }
    x->next = node->xattrs;
    node->xattrs = x;
    return true;
}

typedef struct acl_entry {
    uint16_t tag;
    uint16_t perm;
    uint32_t id;
} acl_entry_t;

static int compare_acl_entries(const void *a, const void *b) {
    const acl_entry_t *x = a;
    const acl_entry_t *y = b;
    if (x->tag != y->tag) {
        return x->tag < y->tag ? -1 : 1;
    }
    return x->id < y->id ? -1 : x->id > y->id;
}

static size_t encode_ext4_acl(acl_entry_t *entries, size_t count, uint8_t *out, int *mask_out) {
    qsort(entries, count, sizeof(*entries), compare_acl_entries);
    uint16_t seen = 0;
    for (size_t i = 0; i < count; i++) {
        seen |= entries[i].tag;
    }
    uint16_t required = ACL_USER_OBJ | ACL_GROUP_OBJ | ACL_OTHER;
    if ((seen & required) != required || ((seen & (ACL_USER | ACL_GROUP)) && !(seen & ACL_MASK))) {
        return 0;
    }
    put_le32(out, 1);
    size_t len = 4;
    *mask_out = -1;
    for (size_t i = 0; i < count; i++) {
        if (entries[i].tag == ACL_MASK) {
            *mask_out = entries[i].perm & 7U;
        }
        put_le16(out + len, entries[i].tag);
        put_le16(out + len + 2, entries[i].perm & 7U);
        len += 4;
        if (entries[i].tag == ACL_USER || entries[i].tag == ACL_GROUP) {
            put_le32(out + len, entries[i].id);
            len += 4;
        }
    }
    return len;
}

static bool store_acl(vfs_node_t *node, uint8_t index, acl_entry_t *entries, size_t count) {
    uint8_t *encoded = malloc(4 + count * 8);
    if (!encoded) {
        return false;
    }
    int mask = -1;
    size_t len = count > 0 ? encode_ext4_acl(entries, count, encoded, &mask) : 0;
    bool ok = len == 0 || node_set_xattr(node, index, "", 0, encoded, len);
    if (ok && len > 0 && index == XATTR_INDEX_POSIX_ACL_ACCESS && mask >= 0) {
        node->mode = (node->mode & ~070U) | ((uint32_t)mask << 3);
    }
    free(encoded);
    return ok;
}

static bool set_posix_acl_xattr(vfs_node_t *node, uint8_t index, const uint8_t *value, size_t size) {
    if (size < 4 || get_le32(value) != 2 || (size - 4) % 8 != 0) {
        return true;
    }
    size_t count = (size - 4) / 8;
    acl_entry_t *entries = malloc((count > 0 ? count : 1) * sizeof(*entries));
    if (!entries) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        const uint8_t *e = value + 4 + i * 8;
        entries[i] = (acl_entry_t){.tag = get_le16(e), .perm = get_le16(e + 2), .id = get_le32(e + 4)};
    }
    bool ok = store_acl(node, index, entries, count);
    free(entries);
    return ok;
}

static uint16_t acl_tag_from_archive(int tag) {
    switch (tag) {
    case ARCHIVE_ENTRY_ACL_USER_OBJ: return ACL_USER_OBJ;
    case ARCHIVE_ENTRY_ACL_USER: return ACL_USER;
    case ARCHIVE_ENTRY_ACL_GROUP_OBJ: return ACL_GROUP_OBJ;
    case ARCHIVE_ENTRY_ACL_GROUP: return ACL_GROUP;
    case ARCHIVE_ENTRY_ACL_MASK: return ACL_MASK;
    case ARCHIVE_ENTRY_ACL_OTHER: return ACL_OTHER;
    default: return 0;
    }
}

static bool capture_archive_acl(struct archive_entry *entry, vfs_node_t *node, int type, uint8_t index) {
    int count = archive_entry_acl_reset(entry, type);
    if (count <= 0) {
        return true;
    }
    acl_entry_t *entries = malloc((size_t)count * sizeof(*entries));
    if (!entries) {
        return false;
    }
    size_t n = 0;
    int entry_type = 0;
    int permset = 0;
    int tag = 0;
    int qual = 0;
    const char *name = NULL;
    while (n < (size_t)count &&
           archive_entry_acl_next(entry, type, &entry_type, &permset, &tag, &qual, &name) == ARCHIVE_OK) {
        uint16_t ext4_tag = acl_tag_from_archive(tag);
        if (ext4_tag != 0) {
            uint16_t perm = (uint16_t)(((permset & ARCHIVE_ENTRY_ACL_READ) ? 4 : 0) |
                                       ((permset & ARCHIVE_ENTRY_ACL_WRITE) ? 2 : 0) |
                                       ((permset & ARCHIVE_ENTRY_ACL_EXECUTE) ? 1 : 0));
            entries[n++] = (acl_entry_t){.tag = ext4_tag, .perm = perm, .id = (uint32_t)qual};
        }
    }
    bool ok = store_acl(node, index, entries, n);
    free(entries);
    return ok;
}

static bool capture_entry_xattrs(struct archive_entry *entry, vfs_node_t *node) {
    vfs_node_free_xattrs(node);
    if (archive_entry_xattr_count(entry) > 0) {
        archive_entry_xattr_reset(entry);
        const char *name = NULL;
        const void *value = NULL;
        size_t size = 0;
        while (archive_entry_xattr_next(entry, &name, &value, &size) == ARCHIVE_OK) {
            const char *suffix = NULL;
            uint8_t index = name ? xattr_index_for(name, &suffix) : 0;
            bool ok = true;
            if (index == XATTR_INDEX_POSIX_ACL_ACCESS || index == XATTR_INDEX_POSIX_ACL_DEFAULT) {
                ok = set_posix_acl_xattr(node, index, value, size);
            } else if (index != 0) {
                ok = node_set_xattr(node, index, suffix, strlen(suffix), value, size);
            }
            if (!ok) {
                return false;
            }
        }
    }
    return capture_archive_acl(entry, node, ARCHIVE_ENTRY_ACL_TYPE_ACCESS, XATTR_INDEX_POSIX_ACL_ACCESS) &&
           capture_archive_acl(entry, node, ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, XATTR_INDEX_POSIX_ACL_DEFAULT);
}

static void set_node_attrs(vfs_node_t *node, struct archive_entry *entry, uint32_t mode) {
    node->mode = mode;
    node->uid = (uint32_t)archive_entry_uid(entry);
    node->gid = (uint32_t)archive_entry_gid(entry);
    node->mtime = (int64_t)archive_entry_mtime(entry);
    node->mtime_nsec = (uint32_t)archive_entry_mtime_nsec(entry);
}

typedef struct deferred_hardlink {
    char *link_path;
    char *target_path;
} deferred_hardlink_t;

typedef struct deferred_hl_list {
    deferred_hardlink_t *items;
    size_t count;
    size_t cap;
} deferred_hl_list_t;

static bool deferred_hl_push(deferred_hl_list_t *list, const char *link_path, const char *target_path) {
    if (list->count == list->cap) {
        size_t next_cap = list->cap ? list->cap * 2 : 64;
        deferred_hardlink_t *next = realloc(list->items, next_cap * sizeof(*next));
        if (!next) {
            return false;
        }
        list->items = next;
        list->cap = next_cap;
    }
    char *lp = strdup(link_path);
    char *tp = strdup(target_path);
    if (!lp || !tp) {
        free(lp);
        free(tp);
        return false;
    }
    list->items[list->count++] = (deferred_hardlink_t){.link_path = lp, .target_path = tp};
    return true;
}

static void deferred_hl_free(deferred_hl_list_t *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].link_path);
        free(list->items[i].target_path);
    }
    free(list->items);
    *list = (deferred_hl_list_t){0};
}

static bool resolve_deferred_hardlinks(vfs_node_t *root, deferred_hl_list_t *list, char **error_message) {
    if (list->count == 0) {
        return true;
    }
    bool *done = calloc(list->count, sizeof(bool));
    if (!done) {
        set_error(error_message, "out of memory resolving hardlinks");
        return false;
    }
    size_t remaining = list->count;
    bool ok = true;
    for (int pass = 0; pass < 8 && remaining > 0 && ok; pass++) {
        size_t progressed = 0;
        for (size_t i = 0; i < list->count && ok; i++) {
            if (done[i]) {
                continue;
            }
            vfs_node_t *target = vfs_lookup(root, list->items[i].target_path);
            if (!target) {
                continue;
            }
            vfs_node_t *parent = NULL;
            const char *leaf = NULL;
            size_t leaf_len = 0;
            if (S_ISDIR(target->mode)) {
                set_error(error_message, "archive contains hardlink to directory");
                ok = false;
            } else if (!vfs_resolve_parent(root, list->items[i].link_path, true, NULL, &parent, &leaf, &leaf_len) ||
                       !parent) {
                set_error(error_message, "invalid hardlink path in archive");
                ok = false;
            } else if (!vfs_dir_set(parent, leaf, leaf_len, target)) {
                set_error(error_message, "out of memory linking deferred hardlink");
                ok = false;
            } else {
                done[i] = true;
                remaining--;
                progressed++;
            }
        }
        if (progressed == 0) {
            break;
        }
    }
    free(done);
    if (ok && remaining > 0) {
        set_error(error_message, "archive contains unresolved hardlink target");
        ok = false;
    }
    return ok;
}

static bool write_file_data(struct archive *a, struct archive_entry *entry, ext4_writer_t *writer,
                            vfs_node_t *node, char **error_message) {
    uint64_t end = 0;
    for (;;) {
        const void *buf = NULL;
        size_t size = 0;
        la_int64_t offset = 0;
        int r = archive_read_data_block(a, &buf, &size, &offset);
        if (r == ARCHIVE_EOF) {
            break;
        }
        if (r < ARCHIVE_WARN || offset < 0) {
            set_error(error_message, archive_error_string(a));
            return false;
        }
        if (size == 0) {
            continue;
        }
        int rc = ext4_writer_write(writer, node, (uint64_t)offset, buf, size);
        if (rc < 0) {
            set_errno_error(error_message, "failed to write root disk", -rc);
            return false;
        }
        if ((uint64_t)offset + size > end) {
            end = (uint64_t)offset + size;
        }
    }
    uint64_t size = end;
    if (archive_entry_size_is_set(entry) && archive_entry_size(entry) > 0 &&
        (uint64_t)archive_entry_size(entry) > size) {
        size = (uint64_t)archive_entry_size(entry);
    }
    int rc = ext4_writer_end_file(writer, node, size);
    if (rc < 0) {
        set_errno_error(error_message, "failed to write root disk", -rc);
        return false;
    }
    return true;
}

static void report_progress(struct archive *a, size_t total, const uint64_t *input_pos, void *context,
                            swipterm_progress_handler progress) {
    if (!progress || total == 0) {
        return;
    }
    double done = (double)(input_pos ? *input_pos : (uint64_t)archive_filter_bytes(a, -1));
    double frac = done / (double)total * 0.9;
    progress(context, frac < 0.0 ? 0.0 : frac > 0.9 ? 0.9 : frac);
}

static bool import_entries(struct archive *a, size_t total, const uint64_t *input_pos, ext4_writer_t *writer,
                           vfs_node_t *root, void *context, swipterm_progress_handler progress,
                           char **error_message) {
    deferred_hl_list_t deferred = {0};
    size_t entry_count = 0;
    struct archive_entry *entry = NULL;
    int r;
    bool ok = true;
    while (ok && ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK || r == ARCHIVE_WARN)) {
        if (++entry_count > VFS_MAX_ENTRIES) {
            set_error(error_message, "archive exceeds maximum entry limit");
            ok = false;
            break;
        }
        const char *pathname = archive_entry_pathname(entry);
        if (!pathname || pathname[0] == '\0') {
            continue;
        }
        vfs_path_stack_t parent_stack;
        vfs_node_t *parent = NULL;
        const char *leaf = NULL;
        size_t leaf_len = 0;
        if (!vfs_resolve_parent(root, pathname, true, &parent_stack, &parent, &leaf, &leaf_len)) {
            set_error(error_message, "invalid path in archive");
            ok = false;
            break;
        }
        uint32_t mode = (uint32_t)archive_entry_mode(entry);
        if ((mode & S_IFMT) == 0) {
            mode |= S_IFREG;
        }
        if (!parent) {
            set_node_attrs(root, entry, (mode & 07777U) | S_IFDIR);
            if (!capture_entry_xattrs(entry, root)) {
                set_error(error_message, "out of memory reading root xattrs");
                ok = false;
            }
            continue;
        }

        if (leaf_len == 12 && memcmp(leaf, ".wh..wh..opq", 12) == 0) {
            vfs_dir_clear(parent);
            continue;
        }
        if (leaf_len >= 8 && memcmp(leaf, ".wh..wh.", 8) == 0) {
            continue;
        }
        if (leaf_len > 4 && memcmp(leaf, ".wh.", 4) == 0) {
            vfs_dir_remove(parent, leaf + 4, leaf_len - 4);
            continue;
        }

        const char *hardlink = archive_entry_hardlink(entry);
        if (hardlink && hardlink[0] != '\0') {
            vfs_node_t *target = vfs_lookup(root, hardlink);
            if (!target) {
                if (!deferred_hl_push(&deferred, pathname, hardlink)) {
                    set_error(error_message, "out of memory queuing deferred hardlink");
                    ok = false;
                }
            } else if (S_ISDIR(target->mode)) {
                set_error(error_message, "archive contains hardlink to directory");
                ok = false;
            } else if (!vfs_dir_set(parent, leaf, leaf_len, target)) {
                set_error(error_message, "out of memory linking hardlink");
                ok = false;
            }
            continue;
        }

        vfs_dirent_t *existing = vfs_dir_find(parent, leaf, leaf_len);
        if (S_ISDIR(mode) && existing && S_ISLNK(existing->node->mode)) {
            vfs_path_stack_t target_st = parent_stack;
            unsigned sym_depth = 0;
            if (vfs_follow_symlink_dir(root, &target_st, existing->node->symlink, true, &sym_depth)) {
                vfs_node_t *tdir = target_st.nodes[target_st.depth];
                if (tdir->implicit_dir) {
                    set_node_attrs(tdir, entry, mode);
                    tdir->implicit_dir = false;
                }
            }
            continue;
        }
        if (S_ISLNK(mode) && existing && S_ISDIR(existing->node->mode)) {
            const char *sym_target = archive_entry_symlink(entry);
            if (sym_target && sym_target[0] != '\0') {
                vfs_path_stack_t target_st = parent_stack;
                unsigned sym_depth = 0;
                if (vfs_follow_symlink_dir(root, &target_st, sym_target, true, &sym_depth)) {
                    vfs_node_t *tdir = target_st.nodes[target_st.depth];
                    if (tdir != existing->node && S_ISDIR(tdir->mode) && !vfs_dir_merge(tdir, existing->node, 0)) {
                        set_error(error_message, "out of memory merging symlinked directory");
                        ok = false;
                        continue;
                    }
                }
            }
        }

        vfs_node_t *node = NULL;
        if (S_ISDIR(mode) && existing && S_ISDIR(existing->node->mode)) {
            node = vfs_node_retain(existing->node);
            node->implicit_dir = false;
        } else {
            node = vfs_node_new(mode);
            if (!node) {
                set_error(error_message, "out of memory");
                ok = false;
                continue;
            }
        }
        set_node_attrs(node, entry, mode);
        if (!capture_entry_xattrs(entry, node)) {
            set_error(error_message, "out of memory reading xattrs");
            ok = false;
        } else if (S_ISREG(mode)) {
            ok = write_file_data(a, entry, writer, node, error_message);
        } else if (S_ISLNK(mode)) {
            const char *target = archive_entry_symlink(entry);
            size_t tlen = target ? strlen(target) : 0;
            if (tlen == 0 || tlen > VFS_MAX_SYMLINK_LEN) {
                set_error(error_message, "invalid symlink target in archive");
                ok = false;
            } else {
                free(node->symlink);
                node->symlink = strdup(target);
                if (!node->symlink) {
                    set_error(error_message, "out of memory");
                    ok = false;
                }
            }
        } else if (S_ISCHR(mode) || S_ISBLK(mode)) {
            node->rdev_major = archive_entry_rdevmajor(entry);
            node->rdev_minor = archive_entry_rdevminor(entry);
        }
        if (ok && !vfs_dir_set(parent, leaf, leaf_len, node)) {
            set_error(error_message, "out of memory inserting entry");
            ok = false;
        }
        vfs_node_release(node);
        report_progress(a, total, input_pos, context, progress);
    }
    if (ok && r != ARCHIVE_EOF) {
        set_error(error_message, archive_error_string(a));
        ok = false;
    }
    if (ok && entry_count == 0) {
        set_error(error_message, "archive is empty");
        ok = false;
    }
    ok = ok && resolve_deferred_hardlinks(root, &deferred, error_message);
    deferred_hl_free(&deferred);
    return ok;
}

static int fsync_path(const char *path) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -errno;
    }
    int rc = fsync(fd) == 0 ? 0 : -errno;
    close(fd);
    return rc;
}

static bool write_metadata(const char *root_path, char **error_message) {
    static const char json[] = "{\"version\":2,\"disk\":\"root.ext4\"}\n";
    char tmp_path[1024];
    char final_path[1024];
    if (snprintf(tmp_path, sizeof(tmp_path), "%s/swipterm.json.partial", root_path) >= (int)sizeof(tmp_path) ||
        snprintf(final_path, sizeof(final_path), "%s/swipterm.json", root_path) >= (int)sizeof(final_path)) {
        set_error(error_message, "root path too long");
        return false;
    }
    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        set_errno_error(error_message, "failed to write swipterm.json", errno);
        return false;
    }
    bool ok = write(fd, json, sizeof(json) - 1) == (ssize_t)(sizeof(json) - 1) && fsync(fd) == 0;
    int err = errno;
    ok = close(fd) == 0 && ok;
    if (ok && rename(tmp_path, final_path) != 0) {
        err = errno;
        ok = false;
    }
    if (!ok) {
        unlink(tmp_path);
        set_errno_error(error_message, "failed to write swipterm.json", err);
    }
    return ok;
}

static void remove_stale(const char *root_path) {
    static const char *const names[] = {"lower.erofs", "upper.ext4", ".preboot_snapshot", ".preboot_snapshot.bundled"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char path[1024];
        if (snprintf(path, sizeof(path), "%s/%s", root_path, names[i]) < (int)sizeof(path)) {
            (void)unlink(path);
        }
    }
}

static bool import_root(struct archive *a, size_t total, const uint64_t *input_pos, const char *root_path,
                        uint64_t disk_size, void *context, swipterm_progress_handler progress,
                        char **error_message) {
    char partial_path[1024];
    char disk_path[1024];
    if (snprintf(partial_path, sizeof(partial_path), "%s/root.ext4.partial", root_path) >= (int)sizeof(partial_path) ||
        snprintf(disk_path, sizeof(disk_path), "%s/root.ext4", root_path) >= (int)sizeof(disk_path)) {
        set_error(error_message, "root path too long");
        return false;
    }
    if (mkdir(root_path, 0755) != 0 && errno != EEXIST) {
        set_errno_error(error_message, "failed to create root directory", errno);
        return false;
    }
    int root_fd = open(root_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (root_fd < 0) {
        set_errno_error(error_message, "failed to open root directory", errno);
        return false;
    }
    if (flock(root_fd, LOCK_EX | LOCK_NB) != 0) {
        set_error(error_message, errno == EWOULDBLOCK ? "the root is used by a running machine" : strerror(errno));
        close(root_fd);
        return false;
    }
    int fd = open(partial_path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        set_errno_error(error_message, "failed to create root disk", errno);
        close(root_fd);
        return false;
    }
    ext4_writer_t *writer = NULL;
    vfs_node_t *root = vfs_node_new(S_IFDIR | 0755);
    int rc = root ? ext4_writer_create(fd, disk_size ? disk_size : DEFAULT_DISK_SIZE, &writer) : -ENOMEM;
    bool ok = rc == 0;
    if (!ok) {
        set_errno_error(error_message, rc == -EINVAL ? "invalid disk size" : "failed to create root disk", -rc);
    }
    ok = ok && import_entries(a, total, input_pos, writer, root, context, progress, error_message);
    if (ok) {
        rc = ext4_writer_finish(writer, root);
        if (rc < 0) {
            set_errno_error(error_message, rc == -ENOSPC ? "the archive does not fit the disk size"
                                                         : "failed to write root disk", -rc);
            ok = false;
        }
    }
    vfs_node_release(root);
    ext4_writer_free(writer);
    if (ok && fsync(fd) != 0) {
        set_errno_error(error_message, "failed to write root disk", errno);
        ok = false;
    }
    if (close(fd) != 0 && ok) {
        set_errno_error(error_message, "failed to write root disk", errno);
        ok = false;
    }
    if (ok && rename(partial_path, disk_path) != 0) {
        set_errno_error(error_message, "failed to install root disk", errno);
        ok = false;
    }
    if (!ok) {
        unlink(partial_path);
    } else {
        remove_stale(root_path);
        ok = write_metadata(root_path, error_message);
    }
    if (ok) {
        (void)fsync(root_fd);
        (void)fsync_path(disk_path);
        if (progress) {
            progress(context, 1.0);
        }
    }
    close(root_fd);
    return ok;
}

static bool has_xz_magic(const uint8_t *hdr, size_t len) {
    static const uint8_t magic[6] = {0xFD, '7', 'z', 'X', 'Z', 0x00};
    return len >= sizeof(magic) && memcmp(hdr, magic, sizeof(magic)) == 0;
}

static bool has_zstd_magic(const uint8_t *hdr, size_t len) {
    return len >= 4 && get_le32(hdr) == ZSTD_MAGICNUMBER;
}

typedef struct xz_mt_reader {
    lzma_stream strm;
    int fd;
    bool input_eof;
    bool stream_end;
    uint8_t *in_buf;
    uint8_t *out_buf;
} xz_mt_reader_t;

static void xz_mt_reader_free(xz_mt_reader_t *r) {
    if (!r) {
        return;
    }
    lzma_end(&r->strm);
    free(r->in_buf);
    free(r->out_buf);
    free(r);
}

static xz_mt_reader_t *xz_mt_reader_new(int fd, const uint8_t *data, size_t size) {
    xz_mt_reader_t *r = calloc(1, sizeof(*r));
    if (!r) {
        return NULL;
    }
    r->fd = fd;
    r->out_buf = malloc(XZ_MT_BUF_SIZE);
    r->in_buf = fd >= 0 ? malloc(XZ_MT_BUF_SIZE) : NULL;
    uint32_t threads = lzma_cputhreads();
    lzma_mt mt = {
        .flags = LZMA_CONCATENATED,
        .threads = threads == 0U ? 4U : threads < 2U ? 2U : threads > 6U ? 6U : threads,
        .memlimit_threading = 128ULL << 20,
        .memlimit_stop = 512ULL << 20,
    };
    if (!r->out_buf || (fd >= 0 && !r->in_buf) || lzma_stream_decoder_mt(&r->strm, &mt) != LZMA_OK) {
        xz_mt_reader_free(r);
        return NULL;
    }
    if (fd < 0) {
        r->strm.next_in = data;
        r->strm.avail_in = size;
        r->input_eof = true;
    }
    return r;
}

static la_ssize_t xz_mt_read(struct archive *a, void *client, const void **buffer) {
    xz_mt_reader_t *r = client;
    r->strm.next_out = r->out_buf;
    r->strm.avail_out = XZ_MT_BUF_SIZE;
    while (!r->stream_end && r->strm.avail_out == XZ_MT_BUF_SIZE) {
        if (r->strm.avail_in == 0 && !r->input_eof) {
            ssize_t n = read(r->fd, r->in_buf, XZ_MT_BUF_SIZE);
            if (n < 0) {
                int err = errno;
                if (err == EINTR) {
                    continue;
                }
                archive_set_error(a, err, "%s", strerror(err));
                return ARCHIVE_FATAL;
            }
            r->strm.next_in = r->in_buf;
            r->strm.avail_in = (size_t)n;
            r->input_eof = n == 0;
        }
        lzma_ret ret = lzma_code(&r->strm, r->input_eof ? LZMA_FINISH : LZMA_RUN);
        if (ret == LZMA_STREAM_END) {
            r->stream_end = true;
        } else if (ret != LZMA_OK) {
            archive_set_error(a, ARCHIVE_ERRNO_MISC, "xz decompression failed (liblzma error %d)", (int)ret);
            return ARCHIVE_FATAL;
        }
    }
    *buffer = r->out_buf;
    return (la_ssize_t)(XZ_MT_BUF_SIZE - r->strm.avail_out);
}

static const uint8_t *xz_tail_bytes(int fd, const uint8_t *data, uint64_t off, size_t len, uint8_t *buf) {
    if (data) {
        return data + off;
    }
    for (size_t done = 0; done < len;) {
        ssize_t n = pread(fd, buf + done, len - done, (off_t)(off + done));
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return NULL;
        }
        done += (size_t)n;
    }
    return buf;
}

static uint64_t xz_uncompressed_size(int fd, const uint8_t *data, uint64_t size) {
    uint8_t tail_buf[XZ_TAIL_SIZE];
    size_t tail_len = size < XZ_TAIL_SIZE ? (size_t)size : XZ_TAIL_SIZE;
    const uint8_t *tail = xz_tail_bytes(fd, data, size - tail_len, tail_len, tail_buf);
    if (!tail) {
        return 0;
    }
    size_t end = tail_len;
    while (end >= 4 && get_le32(tail + end - 4) == 0) {
        end -= 4;
    }
    lzma_stream_flags flags;
    if (end < LZMA_STREAM_HEADER_SIZE ||
        lzma_stream_footer_decode(&flags, tail + end - LZMA_STREAM_HEADER_SIZE) != LZMA_OK) {
        return 0;
    }
    uint64_t padding = tail_len - end;
    uint64_t index_end = size - padding - LZMA_STREAM_HEADER_SIZE;
    if (flags.backward_size > XZ_INDEX_MAX_SIZE || flags.backward_size + LZMA_STREAM_HEADER_SIZE > index_end) {
        return 0;
    }
    size_t index_size = (size_t)flags.backward_size;
    uint8_t *index_buf = data ? NULL : malloc(index_size);
    if (!data && !index_buf) {
        return 0;
    }
    const uint8_t *index_bytes = xz_tail_bytes(fd, data, index_end - index_size, index_size, index_buf);
    uint64_t uncompressed = 0;
    lzma_index *idx = NULL;
    uint64_t memlimit = UINT64_MAX;
    size_t pos = 0;
    if (index_bytes && lzma_index_buffer_decode(&idx, &memlimit, NULL, index_bytes, &pos, index_size) == LZMA_OK) {
        if (pos == index_size && lzma_index_stream_size(idx) + padding == size) {
            uncompressed = lzma_index_uncompressed_size(idx);
        }
        lzma_index_end(idx, NULL);
    }
    free(index_buf);
    return uncompressed;
}

typedef struct zstd_reader {
    ZSTD_DCtx *dctx;
    int fd;
    ZSTD_inBuffer in;
    uint8_t *in_buf;
    uint8_t *out_buf;
    size_t out_cap;
    bool input_eof;
    size_t last_ret;
    uint64_t total_in;
} zstd_reader_t;

static void zstd_reader_free(zstd_reader_t *r) {
    if (!r) {
        return;
    }
    ZSTD_freeDCtx(r->dctx);
    free(r->in_buf);
    free(r->out_buf);
    free(r);
}

static zstd_reader_t *zstd_reader_new(int fd, const uint8_t *data, size_t size) {
    zstd_reader_t *r = calloc(1, sizeof(*r));
    if (!r) {
        return NULL;
    }
    r->fd = fd;
    r->dctx = ZSTD_createDCtx();
    r->out_cap = ZSTD_DStreamOutSize();
    r->out_buf = malloc(r->out_cap);
    r->in_buf = fd >= 0 ? malloc(ZSTD_DStreamInSize()) : NULL;
    if (!r->dctx || !r->out_buf || (fd >= 0 && !r->in_buf)) {
        zstd_reader_free(r);
        return NULL;
    }
    (void)ZSTD_DCtx_setParameter(r->dctx, ZSTD_d_windowLogMax, 31);
    if (fd < 0) {
        r->in = (ZSTD_inBuffer){.src = data, .size = size, .pos = 0};
        r->input_eof = true;
    }
    return r;
}

static la_ssize_t zstd_read(struct archive *a, void *client, const void **buffer) {
    zstd_reader_t *r = client;
    ZSTD_outBuffer out = {.dst = r->out_buf, .size = r->out_cap, .pos = 0};
    while (out.pos == 0) {
        if (r->in.pos == r->in.size && !r->input_eof) {
            ssize_t n = read(r->fd, r->in_buf, ZSTD_DStreamInSize());
            if (n < 0) {
                int err = errno;
                if (err == EINTR) {
                    continue;
                }
                archive_set_error(a, err, "%s", strerror(err));
                return ARCHIVE_FATAL;
            }
            r->in = (ZSTD_inBuffer){.src = r->in_buf, .size = (size_t)n, .pos = 0};
            r->input_eof = n == 0;
        }
        if (r->in.pos == r->in.size && r->input_eof) {
            if (r->last_ret != 0) {
                archive_set_error(a, ARCHIVE_ERRNO_MISC, "truncated zstd stream");
                return ARCHIVE_FATAL;
            }
            break;
        }
        size_t before = r->in.pos;
        size_t ret = ZSTD_decompressStream(r->dctx, &out, &r->in);
        r->total_in += r->in.pos - before;
        if (ZSTD_isError(ret)) {
            archive_set_error(a, ARCHIVE_ERRNO_MISC, "zstd decompression failed: %s", ZSTD_getErrorName(ret));
            return ARCHIVE_FATAL;
        }
        r->last_ret = ret;
    }
    *buffer = r->out_buf;
    return (la_ssize_t)out.pos;
}

static bool setup_native_reader(struct archive *a, char **error_message) {
    (void)archive_read_support_filter_none(a);
    (void)archive_read_support_filter_gzip(a);
    (void)archive_read_support_filter_bzip2(a);
    (void)archive_read_support_filter_xz(a);
    (void)archive_read_support_filter_lzma(a);
    (void)archive_read_support_filter_compress(a);
    if (archive_read_support_format_tar(a) != ARCHIVE_OK) {
        set_error(error_message, archive_error_string(a));
        return false;
    }
    return true;
}

typedef struct archive_source {
    const char *path;
    int fd;
    const uint8_t *data;
    uint64_t size;
} archive_source_t;

static bool import_source(const archive_source_t *src, const char *root_path, uint64_t disk_size, void *context,
                          swipterm_progress_handler progress, char **error_message) {
    uint8_t magic[6] = {0};
    size_t magic_len = 0;
    if (src->data) {
        magic_len = src->size < sizeof(magic) ? (size_t)src->size : sizeof(magic);
        memcpy(magic, src->data, magic_len);
    } else {
        ssize_t n = pread(src->fd, magic, sizeof(magic), 0);
        magic_len = n > 0 ? (size_t)n : 0;
    }

    struct archive *a = archive_read_new();
    if (!a) {
        set_error(error_message, "failed to allocate archive reader");
        return false;
    }
    xz_mt_reader_t *xz = NULL;
    zstd_reader_t *zstd = NULL;
    uint64_t total = src->size;
    const uint64_t *input_pos = NULL;
    int r;
    if (has_xz_magic(magic, magic_len)) {
        xz = xz_mt_reader_new(src->data ? -1 : src->fd, src->data, (size_t)src->size);
        uint64_t uncompressed = xz ? xz_uncompressed_size(src->data ? -1 : src->fd, src->data, src->size) : 0;
        if (uncompressed > 0) {
            total = uncompressed;
        } else if (xz) {
            input_pos = &xz->strm.total_in;
        }
    } else if (has_zstd_magic(magic, magic_len)) {
        zstd = zstd_reader_new(src->data ? -1 : src->fd, src->data, (size_t)src->size);
        if (zstd) {
            input_pos = &zstd->total_in;
        }
    }
    if ((has_xz_magic(magic, magic_len) && !xz) || (has_zstd_magic(magic, magic_len) && !zstd)) {
        set_error(error_message, "failed to initialize decompressor");
        archive_read_free(a);
        xz_mt_reader_free(xz);
        zstd_reader_free(zstd);
        return false;
    }
    if (xz || zstd) {
        r = archive_read_support_filter_none(a);
        if (r == ARCHIVE_OK) {
            r = archive_read_support_format_tar(a);
        }
        if (r == ARCHIVE_OK) {
            r = xz ? archive_read_open(a, xz, NULL, xz_mt_read, NULL)
                   : archive_read_open(a, zstd, NULL, zstd_read, NULL);
        }
    } else if (setup_native_reader(a, error_message)) {
        r = src->data ? archive_read_open_memory(a, src->data, (size_t)src->size)
                      : archive_read_open_filename(a, src->path, STREAM_IO_BUF_SIZE);
    } else {
        r = ARCHIVE_FATAL;
    }
    bool ok = r == ARCHIVE_OK;
    if (!ok) {
        set_error(error_message, archive_error_string(a));
    }
    ok = ok && import_root(a, (size_t)total, input_pos, root_path, disk_size, context, progress, error_message);
    archive_read_free(a);
    xz_mt_reader_free(xz);
    zstd_reader_free(zstd);
    return ok;
}

bool swipterm_root_import_file(const char *archive_path, const char *root_path, uint64_t disk_size,
                               void *context, swipterm_progress_handler progress, char **error_message) {
    if (error_message) {
        *error_message = NULL;
    }
    if (!archive_path || !root_path) {
        set_error(error_message, "invalid arguments");
        return false;
    }
    int fd = open(archive_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        set_errno_error(error_message, "failed to open archive", errno);
        return false;
    }
    struct stat st;
    if (fstat(fd, &st) != 0) {
        set_errno_error(error_message, "failed to open archive", errno);
        close(fd);
        return false;
    }
    archive_source_t src = {.path = archive_path, .fd = fd, .size = (uint64_t)st.st_size};
    bool ok = import_source(&src, root_path, disk_size, context, progress, error_message);
    close(fd);
    return ok;
}

bool swipterm_root_import_memory(const void *data, size_t size, const char *root_path, uint64_t disk_size,
                                 void *context, swipterm_progress_handler progress, char **error_message) {
    if (error_message) {
        *error_message = NULL;
    }
    if (!data || size == 0 || !root_path) {
        set_error(error_message, "invalid arguments");
        return false;
    }
    archive_source_t src = {.fd = -1, .data = data, .size = size};
    return import_source(&src, root_path, disk_size, context, progress, error_message);
}
