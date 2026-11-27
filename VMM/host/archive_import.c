#define _DARWIN_C_SOURCE

#include "host_internal.h"

#include <CommonCrypto/CommonDigest.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum : uint32_t {
    VFS_MAX_SYMLINKS = 40U,
    VFS_MAX_NAME_LEN = 255U,
    VFS_MAX_SYMLINK_LEN = 4095U,
    VFS_MAX_XATTR_VALUE = 4096U,
    VFS_HASH_THRESHOLD = 32U,
    VFS_MAX_ENTRIES = 4000000U,
    PACKAGE_METADATA_LIMIT = 64U << 20,
    RPM_HEADER_LIMIT = 256U << 20,
    BUILDER_READ_LIMIT = 256U << 20,
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

bool vfs_dir_remove(vfs_node_t *dir, const char *name, size_t nlen) {
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
    vfs_node_t **nodes;
    size_t depth;
    size_t cap;
} vfs_path_stack_t;

static bool vfs_path_set(vfs_path_stack_t *st, size_t depth, vfs_node_t *node) {
    if (depth >= st->cap) {
        size_t cap = st->cap ? st->cap * 2 : 64;
        vfs_node_t **nodes = realloc(st->nodes, cap * sizeof(*nodes));
        if (!nodes) {
            return false;
        }
        st->nodes = nodes;
        st->cap = cap;
    }
    st->nodes[depth] = node;
    st->depth = depth;
    return true;
}

static bool vfs_walk_dir_path(vfs_node_t *root, vfs_path_stack_t *st, const char *path,
                              size_t path_len, bool create_dirs, unsigned *symlink_depth);

static bool vfs_follow_symlink_dir(vfs_node_t *root, vfs_path_stack_t *st,
                                   const char *target, bool create_dirs, unsigned *symlink_depth) {
    if (!target || target[0] == '\0' || *symlink_depth >= VFS_MAX_SYMLINKS) {
        return false;
    }
    (*symlink_depth)++;
    if (target[0] == '/' && !vfs_path_set(st, 0, root)) {
        return false;
    }
    return vfs_walk_dir_path(root, st, target, strlen(target), create_dirs, symlink_depth);
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
    return S_ISDIR(de->node->mode) && vfs_path_set(st, st->depth + 1, de->node);
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

static bool vfs_resolve_parent(vfs_node_t *root, vfs_path_stack_t *st, const char *raw_path, bool create_parents,
                               vfs_node_t **parent_out, const char **leaf_out, size_t *leaf_len_out) {
    const char *p = raw_path;
    while (*p == '/' || (p[0] == '.' && p[1] == '/')) {
        p += (*p == '/') ? 1 : 2;
    }
    size_t len = strlen(p);
    while (len > 0 && p[len - 1] == '/') {
        len--;
    }
    *parent_out = NULL;
    *leaf_out = NULL;
    *leaf_len_out = 0;
    if (!vfs_path_set(st, 0, root)) {
        return false;
    }
    if (len == 0 || (len == 1 && p[0] == '.')) {
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
            if (st->depth == 0) {
                return false;
            }
            st->depth--;
            p = slash ? slash + 1 : end;
            continue;
        }
        if (clen > VFS_MAX_NAME_LEN) {
            return false;
        }
        vfs_node_t *cur = st->nodes[st->depth];
        if (!S_ISDIR(cur->mode)) {
            return false;
        }
        if (!slash) {
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
        if (!vfs_enter(root, st, de, create_parents, &sym_depth)) {
            return false;
        }
        p = slash + 1;
    }
    return true;
}

static vfs_node_t *vfs_lookup(vfs_node_t *root, vfs_path_stack_t *st, const char *raw_path) {
    vfs_node_t *parent = NULL;
    const char *leaf = NULL;
    size_t leaf_len = 0;
    if (!vfs_resolve_parent(root, st, raw_path, false, &parent, &leaf, &leaf_len)) {
        return NULL;
    }
    if (!parent) {
        return root;
    }
    vfs_dirent_t *de = vfs_dir_find(parent, leaf, leaf_len);
    return de ? de->node : NULL;
}

typedef struct vfs_merge_frame {
    vfs_node_t *dst;
    vfs_node_t *src;
    size_t index;
} vfs_merge_frame_t;

static bool vfs_dir_merge(vfs_node_t *dst, vfs_node_t *src) {
    size_t cap = 64;
    size_t top = 0;
    vfs_merge_frame_t *stack = malloc(cap * sizeof(*stack));
    if (!stack) {
        return false;
    }
    stack[top++] = (vfs_merge_frame_t){dst, src, 0};
    bool ok = true;
    while (ok && top > 0) {
        vfs_merge_frame_t *frame = &stack[top - 1];
        if (frame->dst == frame->src || frame->index >= frame->src->child_count) {
            top--;
            continue;
        }
        const vfs_dirent_t *se = &frame->src->children[frame->index++];
        vfs_dirent_t *de = vfs_dir_find(frame->dst, se->name, se->name_len);
        if (!de) {
            ok = vfs_dir_set(frame->dst, se->name, se->name_len, se->node);
        } else if (S_ISDIR(de->node->mode) && S_ISDIR(se->node->mode)) {
            if (top == cap) {
                vfs_merge_frame_t *next = realloc(stack, cap * 2 * sizeof(*next));
                if (!next) {
                    ok = false;
                    break;
                }
                stack = next;
                cap *= 2;
            }
            stack[top++] = (vfs_merge_frame_t){de->node, se->node, 0};
        }
    }
    free(stack);
    return ok;
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

static bool resolve_deferred_hardlinks(vfs_node_t *root, vfs_path_stack_t *st, deferred_hl_list_t *list,
                                       char **error_message) {
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
    size_t progressed = 1;
    while (remaining > 0 && ok && progressed > 0) {
        progressed = 0;
        for (size_t i = 0; i < list->count && ok; i++) {
            if (done[i]) {
                continue;
            }
            vfs_node_t *target = vfs_lookup(root, st, list->items[i].target_path);
            if (!target) {
                continue;
            }
            vfs_node_t *parent = NULL;
            const char *leaf = NULL;
            size_t leaf_len = 0;
            if (S_ISDIR(target->mode)) {
                set_error(error_message, "archive contains hardlink to directory");
                ok = false;
            } else if (!vfs_resolve_parent(root, st, list->items[i].link_path, true, &parent, &leaf, &leaf_len) ||
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
    }
    free(done);
    if (ok && remaining > 0) {
        set_error(error_message, "archive contains unresolved hardlink target");
        ok = false;
    }
    return ok;
}

vfs_node_t *vfs_find(vfs_node_t *root, const char *path, bool follow_final) {
    vfs_path_stack_t st = {0};
    char *current = strdup(path);
    vfs_node_t *found = NULL;
    for (unsigned depth = 0; current && depth <= VFS_MAX_SYMLINKS; depth++) {
        vfs_node_t *node = vfs_lookup(root, &st, current);
        if (!node || !follow_final || !S_ISLNK(node->mode)) {
            found = node;
            break;
        }
        const char *target = node->symlink;
        char *next = NULL;
        if (target[0] == '/') {
            next = strdup(target);
        } else {
            const char *slash = strrchr(current, '/');
            size_t dir_len = slash ? (size_t)(slash - current) : 0;
            size_t target_len = strlen(target);
            next = malloc(dir_len + 1 + target_len + 1);
            if (next) {
                memcpy(next, current, dir_len);
                next[dir_len] = '/';
                memcpy(next + dir_len + 1, target, target_len + 1);
            }
        }
        free(current);
        current = next;
    }
    free(current);
    free(st.nodes);
    return found;
}

int vfs_write_file(ext4_writer_t *w, vfs_node_t *dir, const char *name, size_t name_len, const void *data,
                   size_t size, uint32_t mode) {
    vfs_node_t *node = vfs_node_new(S_IFREG | (mode & 07777U));
    if (!node) {
        return -ENOMEM;
    }
    int rc = size > 0 ? ext4_writer_write(w, node, 0, data, size) : 0;
    if (rc == 0) {
        rc = ext4_writer_end_file(w, node, size);
    }
    if (rc == 0 && !vfs_dir_set(dir, name, name_len, node)) {
        rc = -ENOMEM;
    }
    vfs_node_release(node);
    return rc;
}

struct swipterm_root_builder {
    char *root_path;
    char *partial_path;
    int root_fd;
    int disk_fd;
    ext4_writer_t *writer;
    vfs_node_t *root;
    vfs_path_stack_t st;
    bool failed;
    bool finished;
};

typedef struct import_job {
    swipterm_root_builder *builder;
    struct archive *top_archive;
    const archive_stream_t *top_stream;
    swipterm_progress_handler progress;
    void *context;
    const swipterm_package_callbacks *package;
    bool top_level_metadata;
    char **error_message;
} import_job_t;

static bool report_progress(const import_job_t *job) {
    if (!job->progress) {
        return true;
    }
    double fraction = archive_stream_progress(job->top_archive, job->top_stream);
    if (job->progress(job->context, fraction < 0.0 ? 0.0 : fraction > 0.999 ? 0.999 : fraction)) {
        return true;
    }
    set_error(job->error_message, strerror(ECANCELED));
    return false;
}

static const char *strip_path(const char *path) {
    while (*path == '/' || (path[0] == '.' && path[1] == '/')) {
        path += *path == '/' ? 1 : 2;
    }
    return path[0] == '.' && path[1] == '\0' ? path + 1 : path;
}

static void report_entry(const import_job_t *job, const char *pathname, uint32_t mode, const uint8_t *sha1) {
    if (!job->package || !job->package->entry) {
        return;
    }
    const char *stripped = strip_path(pathname);
    size_t len = strlen(stripped);
    while (len > 0 && stripped[len - 1] == '/') {
        len--;
    }
    char *path = malloc(len + 2);
    if (!path) {
        return;
    }
    path[0] = '/';
    memcpy(path + 1, stripped, len);
    path[len + 1] = '\0';
    job->package->entry(job->package->context, path, mode, sha1);
    free(path);
}

static bool read_entry_data(const import_job_t *job, struct archive *a, uint8_t **data_out, size_t *size_out) {
    size_t cap = 65536;
    size_t size = 0;
    uint8_t *data = malloc(cap);
    for (;;) {
        if (!data) {
            set_error(job->error_message, strerror(ENOMEM));
            return false;
        }
        if (size == cap) {
            if (cap >= PACKAGE_METADATA_LIMIT) {
                free(data);
                set_error(job->error_message, "package metadata is too large");
                return false;
            }
            cap *= 2;
            uint8_t *grown = realloc(data, cap);
            if (!grown) {
                free(data);
            }
            data = grown;
            continue;
        }
        la_ssize_t n = archive_read_data(a, data + size, cap - size);
        if (n < 0) {
            set_error(job->error_message, archive_error_string(a));
            free(data);
            return false;
        }
        if (n == 0) {
            break;
        }
        size += (size_t)n;
    }
    *data_out = data;
    *size_out = size;
    return true;
}

static bool report_metadata(const import_job_t *job, struct archive *a, const char *pathname) {
    uint8_t *data = NULL;
    size_t size = 0;
    if (!read_entry_data(job, a, &data, &size)) {
        return false;
    }
    if (job->package->metadata) {
        const char *name = strip_path(pathname);
        const char *slash = strrchr(name, '/');
        job->package->metadata(job->package->context, slash ? slash + 1 : name, data, size);
    }
    free(data);
    return true;
}

static void sha1_zeros(CC_SHA1_CTX *context, uint64_t length) {
    static const uint8_t zeros[65536];
    while (length > 0) {
        size_t n = length < sizeof(zeros) ? (size_t)length : sizeof(zeros);
        CC_SHA1_Update(context, zeros, (CC_LONG)n);
        length -= n;
    }
}

static bool write_file_data(const import_job_t *job, struct archive *a, struct archive_entry *entry,
                            vfs_node_t *node, uint8_t *sha1) {
    ext4_writer_t *writer = job->builder->writer;
    CC_SHA1_CTX context;
    CC_SHA1_Init(&context);
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
            set_error(job->error_message, archive_error_string(a));
            return false;
        }
        if (size == 0) {
            continue;
        }
        int rc = ext4_writer_write(writer, node, (uint64_t)offset, buf, size);
        if (rc < 0) {
            set_errno_error(job->error_message, "failed to write root disk", -rc);
            return false;
        }
        if (sha1) {
            sha1_zeros(&context, (uint64_t)offset - end);
            CC_SHA1_Update(&context, buf, (CC_LONG)size);
        }
        end = (uint64_t)offset + size;
        if (!report_progress(job)) {
            return false;
        }
    }
    uint64_t size = end;
    if (archive_entry_size_is_set(entry) && archive_entry_size(entry) > 0 &&
        (uint64_t)archive_entry_size(entry) > size) {
        size = (uint64_t)archive_entry_size(entry);
    }
    int rc = ext4_writer_end_file(writer, node, size);
    if (rc < 0) {
        set_errno_error(job->error_message, "failed to write root disk", -rc);
        return false;
    }
    if (sha1) {
        sha1_zeros(&context, size - end);
        CC_SHA1_Final(sha1, &context);
    }
    return true;
}

static bool import_entries(const import_job_t *job, struct archive *a, bool require_entries) {
    vfs_node_t *root = job->builder->root;
    vfs_path_stack_t *st = &job->builder->st;
    char **error_message = job->error_message;
    bool package = job->package != NULL;
    bool digest = package && job->package->entry;
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
        uint32_t mode = (uint32_t)archive_entry_mode(entry);
        if ((mode & S_IFMT) == 0) {
            mode |= S_IFREG;
        }
        if (job->top_level_metadata && S_ISREG(mode) && !strchr(strip_path(pathname), '/')) {
            ok = report_metadata(job, a, pathname);
            continue;
        }
        vfs_node_t *parent = NULL;
        const char *leaf = NULL;
        size_t leaf_len = 0;
        if (!vfs_resolve_parent(root, st, pathname, true, &parent, &leaf, &leaf_len)) {
            set_error(error_message, "invalid path in archive");
            ok = false;
            break;
        }
        if (!parent) {
            if (!package) {
                set_node_attrs(root, entry, (mode & 07777U) | S_IFDIR);
                if (!capture_entry_xattrs(entry, root)) {
                    set_error(error_message, "out of memory reading root xattrs");
                    ok = false;
                }
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

        uint8_t sha1[CC_SHA1_DIGEST_LENGTH];
        const uint8_t *entry_digest = NULL;
        const char *hardlink = archive_entry_hardlink(entry);
        if (hardlink && hardlink[0] != '\0') {
            vfs_node_t *target = vfs_lookup(root, st, hardlink);
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
            } else if (archive_entry_size(entry) > 0 && S_ISREG(target->mode) && target->size == 0) {
                ok = write_file_data(job, a, entry, target, digest ? sha1 : NULL);
                entry_digest = digest ? sha1 : NULL;
            }
            if (ok) {
                report_entry(job, pathname, mode, entry_digest);
            }
            continue;
        }

        vfs_dirent_t *existing = vfs_dir_find(parent, leaf, leaf_len);
        if (S_ISDIR(mode) && existing && S_ISLNK(existing->node->mode)) {
            unsigned sym_depth = 0;
            if (vfs_follow_symlink_dir(root, st, existing->node->symlink, true, &sym_depth)) {
                vfs_node_t *tdir = st->nodes[st->depth];
                if (tdir->implicit_dir) {
                    set_node_attrs(tdir, entry, mode);
                    tdir->implicit_dir = false;
                }
            }
            report_entry(job, pathname, mode, NULL);
            continue;
        }
        if (S_ISLNK(mode) && existing && S_ISDIR(existing->node->mode)) {
            const char *sym_target = archive_entry_symlink(entry);
            if (sym_target && sym_target[0] != '\0') {
                unsigned sym_depth = 0;
                bool inside = false;
                if (vfs_follow_symlink_dir(root, st, sym_target, true, &sym_depth)) {
                    for (size_t i = 0; i <= st->depth && !inside; i++) {
                        inside = st->nodes[i] == existing->node;
                    }
                    vfs_node_t *tdir = st->nodes[st->depth];
                    if (!inside && S_ISDIR(tdir->mode) && !vfs_dir_merge(tdir, existing->node)) {
                        set_error(error_message, "out of memory merging symlinked directory");
                        ok = false;
                        continue;
                    }
                }
            }
        }

        vfs_node_t *node = NULL;
        bool keeps_attributes = false;
        if (S_ISDIR(mode) && existing && S_ISDIR(existing->node->mode)) {
            node = vfs_node_retain(existing->node);
            keeps_attributes = package && !node->implicit_dir;
            node->implicit_dir = false;
        } else {
            node = vfs_node_new(mode);
            if (!node) {
                set_error(error_message, "out of memory");
                ok = false;
                continue;
            }
        }
        if (!keeps_attributes) {
            set_node_attrs(node, entry, mode);
            ok = capture_entry_xattrs(entry, node);
            if (!ok) {
                set_error(error_message, "out of memory reading xattrs");
            }
        }
        if (!ok) {
        } else if (S_ISREG(mode)) {
            ok = write_file_data(job, a, entry, node, digest ? sha1 : NULL);
            entry_digest = digest ? sha1 : NULL;
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
                CC_SHA1(target, (CC_LONG)tlen, sha1);
                entry_digest = digest ? sha1 : NULL;
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
        if (ok) {
            report_entry(job, pathname, mode, entry_digest);
        }
        ok = ok && report_progress(job);
    }
    if (ok && r != ARCHIVE_EOF) {
        set_error(error_message, archive_error_string(a));
        ok = false;
    }
    if (ok && require_entries && entry_count == 0) {
        set_error(error_message, "archive is empty");
        ok = false;
    }
    ok = ok && resolve_deferred_hardlinks(root, st, &deferred, error_message);
    deferred_hl_free(&deferred);
    return ok;
}

static bool read_control(const import_job_t *job, struct archive *a) {
    struct archive_entry *entry = NULL;
    int r;
    bool ok = true;
    while (ok && ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK || r == ARCHIVE_WARN)) {
        if ((archive_entry_mode(entry) & S_IFMT) == S_IFREG) {
            ok = report_metadata(job, a, archive_entry_pathname(entry));
        }
    }
    if (ok && r != ARCHIVE_EOF) {
        set_error(job->error_message, archive_error_string(a));
        ok = false;
    }
    return ok;
}

static bool usable(swipterm_root_builder *b, char **error_message) {
    if (error_message) {
        *error_message = NULL;
    }
    if (!b->failed && !b->finished) {
        return true;
    }
    set_error(error_message, "the root image can no longer be changed");
    return false;
}

static bool import_archive(swipterm_root_builder *b, const archive_input_t *input, int (*format)(struct archive *),
                           void *context, swipterm_progress_handler progress,
                           const swipterm_package_callbacks *package, bool top_level_metadata, char **error_message) {
    struct archive *a = archive_read_new();
    archive_stream_t stream = {0};
    bool ok = a && format(a) == ARCHIVE_OK && archive_stream_open(&stream, a, input) == ARCHIVE_OK;
    if (!ok) {
        set_error(error_message, a ? archive_error_string(a) : "failed to allocate archive reader");
    }
    import_job_t job = {
        .builder = b,
        .top_archive = a,
        .top_stream = &stream,
        .progress = progress,
        .context = context,
        .package = package,
        .top_level_metadata = top_level_metadata,
        .error_message = error_message,
    };
    ok = ok && import_entries(&job, a, !package);
    archive_read_free(a);
    archive_stream_close(&stream);
    b->failed = !ok;
    return ok;
}

static bool import_deb(swipterm_root_builder *b, const archive_input_t *input,
                       const swipterm_package_callbacks *callbacks, char **error_message) {
    struct archive *outer = archive_read_new();
    archive_stream_t outer_stream = {0};
    bool ok = outer && archive_read_support_format_ar(outer) == ARCHIVE_OK &&
              archive_stream_open(&outer_stream, outer, input) == ARCHIVE_OK;
    if (!ok) {
        set_error(error_message, outer ? archive_error_string(outer) : "failed to allocate archive reader");
    }
    import_job_t job = {
        .builder = b,
        .top_archive = outer,
        .top_stream = &outer_stream,
        .progress = callbacks->progress,
        .context = callbacks->context,
        .package = callbacks,
        .error_message = error_message,
    };
    bool has_data = false;
    struct archive_entry *entry = NULL;
    int r = ARCHIVE_EOF;
    while (ok && ((r = archive_read_next_header(outer, &entry)) == ARCHIVE_OK || r == ARCHIVE_WARN)) {
        const char *name = archive_entry_pathname(entry);
        bool control = name && strncmp(name, "control.tar", 11) == 0;
        bool data = name && strncmp(name, "data.tar", 8) == 0;
        if (!control && !data) {
            continue;
        }
        struct archive *inner = archive_read_new();
        archive_stream_t inner_stream = {0};
        archive_input_t member = {.fd = -1, .entry = outer};
        ok = inner && archive_read_support_format_tar(inner) == ARCHIVE_OK &&
             archive_stream_open(&inner_stream, inner, &member) == ARCHIVE_OK;
        if (!ok) {
            set_error(error_message, inner ? archive_error_string(inner) : "failed to allocate archive reader");
        }
        ok = ok && (control ? read_control(&job, inner) : import_entries(&job, inner, false));
        has_data = has_data || data;
        archive_read_free(inner);
        archive_stream_close(&inner_stream);
    }
    if (ok && r != ARCHIVE_EOF) {
        set_error(error_message, archive_error_string(outer));
        ok = false;
    }
    if (ok && !has_data) {
        set_error(error_message, "the Debian package has no data archive");
        ok = false;
    }
    archive_read_free(outer);
    archive_stream_close(&outer_stream);
    b->failed = !ok;
    return ok;
}

static uint32_t get_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static bool rpm_payload_offset(int fd, uint64_t size, uint64_t *offset_out, char **error_message) {
    static const uint8_t header_magic[4] = {0x8E, 0xAD, 0xE8, 0x01};
    uint64_t offset = 96;
    for (int header = 0; header < 2; header++) {
        uint8_t intro[16];
        if (pread(fd, intro, sizeof(intro), (off_t)offset) != (ssize_t)sizeof(intro) ||
            memcmp(intro, header_magic, sizeof(header_magic)) != 0) {
            set_error(error_message, "the RPM package header is invalid");
            return false;
        }
        uint64_t length = 16 + 16 * (uint64_t)get_be32(intro + 8) + get_be32(intro + 12);
        if (length > RPM_HEADER_LIMIT) {
            set_error(error_message, "the RPM package header is invalid");
            return false;
        }
        offset += length;
        if (header == 0) {
            offset = (offset + 7) & ~(uint64_t)7;
        }
    }
    if (offset >= size) {
        set_error(error_message, "the RPM package has no payload");
        return false;
    }
    *offset_out = offset;
    return true;
}

swipterm_root_builder *swipterm_root_builder_create(const char *root_path, uint64_t disk_size, char **error_message) {
    if (error_message) {
        *error_message = NULL;
    }
    swipterm_root_builder *b = calloc(1, sizeof(*b));
    if (!b) {
        set_error(error_message, strerror(ENOMEM));
        return NULL;
    }
    b->root_fd = -1;
    b->disk_fd = -1;
    b->root_path = strdup(root_path);
    if (b->root_path && asprintf(&b->partial_path, "%s/root.ext4.partial", root_path) < 0) {
        b->partial_path = NULL;
    }
    if (!b->root_path || !b->partial_path) {
        set_error(error_message, strerror(ENOMEM));
        swipterm_root_builder_free(b);
        return NULL;
    }
    if (mkdir(root_path, 0755) != 0 && errno != EEXIST) {
        set_errno_error(error_message, "failed to create root directory", errno);
        swipterm_root_builder_free(b);
        return NULL;
    }
    b->root_fd = open(root_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (b->root_fd < 0) {
        set_errno_error(error_message, "failed to open root directory", errno);
        swipterm_root_builder_free(b);
        return NULL;
    }
    if (flock(b->root_fd, LOCK_EX | LOCK_NB) != 0) {
        set_error(error_message, errno == EWOULDBLOCK ? "the root is used by a running machine" : strerror(errno));
        close(b->root_fd);
        b->root_fd = -1;
        swipterm_root_builder_free(b);
        return NULL;
    }
    b->disk_fd = open(b->partial_path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (b->disk_fd < 0) {
        set_errno_error(error_message, "failed to create root disk", errno);
        swipterm_root_builder_free(b);
        return NULL;
    }
    b->root = vfs_node_new(S_IFDIR | 0755);
    int rc = b->root ? ext4_writer_create(b->disk_fd, disk_size ? disk_size : DEFAULT_DISK_SIZE, &b->writer) : -ENOMEM;
    if (rc < 0) {
        set_errno_error(error_message, rc == -EINVAL ? "invalid disk size" : "failed to create root disk", -rc);
        swipterm_root_builder_free(b);
        return NULL;
    }
    return b;
}

void swipterm_root_builder_free(swipterm_root_builder *b) {
    if (!b) {
        return;
    }
    vfs_node_release(b->root);
    ext4_writer_free(b->writer);
    free(b->st.nodes);
    if (b->disk_fd >= 0) {
        close(b->disk_fd);
    }
    if (!b->finished && b->partial_path && b->root_fd >= 0) {
        unlink(b->partial_path);
    }
    if (b->root_fd >= 0) {
        close(b->root_fd);
    }
    free(b->partial_path);
    free(b->root_path);
    free(b);
}

bool swipterm_root_builder_add_file(swipterm_root_builder *b, const char *archive_path, void *context,
                                    swipterm_progress_handler progress, char **error_message) {
    if (!usable(b, error_message)) {
        return false;
    }
    int fd = open(archive_path, O_RDONLY | O_CLOEXEC);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) {
        set_errno_error(error_message, "failed to open archive", errno);
        if (fd >= 0) {
            close(fd);
        }
        return false;
    }
    archive_input_t input = {.fd = fd, .size = (uint64_t)st.st_size};
    bool ok = import_archive(b, &input, archive_read_support_format_tar, context, progress, NULL, false, error_message);
    close(fd);
    return ok;
}

bool swipterm_root_builder_add_memory(swipterm_root_builder *b, const void *data, size_t size, void *context,
                                      swipterm_progress_handler progress, char **error_message) {
    if (!usable(b, error_message)) {
        return false;
    }
    if (!data || size == 0) {
        set_error(error_message, "archive is empty");
        return false;
    }
    archive_input_t input = {.fd = -1, .data = data, .size = size};
    return import_archive(b, &input, archive_read_support_format_tar, context, progress, NULL, false, error_message);
}

bool swipterm_root_builder_add_package(swipterm_root_builder *b, const char *package_path,
                                       const swipterm_package_callbacks *callbacks, char **error_message) {
    if (!usable(b, error_message)) {
        return false;
    }
    int fd = open(package_path, O_RDONLY | O_CLOEXEC);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) {
        set_errno_error(error_message, "failed to open package", errno);
        if (fd >= 0) {
            close(fd);
        }
        return false;
    }
    static const uint8_t rpm_magic[4] = {0xED, 0xAB, 0xEE, 0xDB};
    uint8_t magic[8] = {0};
    ssize_t magic_len = pread(fd, magic, sizeof(magic), 0);
    archive_input_t input = {.fd = fd, .size = (uint64_t)st.st_size};
    bool ok;
    if (magic_len == (ssize_t)sizeof(magic) && memcmp(magic, "!<arch>\n", 8) == 0) {
        ok = import_deb(b, &input, callbacks, error_message);
    } else if (magic_len >= 4 && memcmp(magic, rpm_magic, sizeof(rpm_magic)) == 0) {
        ok = rpm_payload_offset(fd, input.size, &input.offset, error_message);
        input.size -= input.offset;
        ok = ok && import_archive(b, &input, archive_read_support_format_cpio, callbacks->context,
                                  callbacks->progress, callbacks, false, error_message);
        b->failed = !ok;
    } else {
        ok = import_archive(b, &input, archive_read_support_format_tar, callbacks->context, callbacks->progress,
                            callbacks, true, error_message);
    }
    close(fd);
    return ok;
}

int swipterm_root_builder_read(swipterm_root_builder *b, const char *path, void **data_out, size_t *size_out) {
    *data_out = NULL;
    *size_out = 0;
    if (b->failed || b->finished) {
        return -LINUX_EINVAL;
    }
    vfs_node_t *node = vfs_find(b->root, path, true);
    if (!node) {
        return -LINUX_ENOENT;
    }
    if (S_ISDIR(node->mode)) {
        return -LINUX_EISDIR;
    }
    if (!S_ISREG(node->mode)) {
        return -LINUX_EINVAL;
    }
    if (node->size > BUILDER_READ_LIMIT) {
        return -LINUX_ENOMEM;
    }
    size_t size = (size_t)node->size;
    uint8_t *data = malloc(size > 0 ? size : 1);
    if (!data) {
        return -LINUX_ENOMEM;
    }
    if (ext4_writer_read(b->writer, node, 0, data, size) < 0) {
        free(data);
        return -LINUX_EIO;
    }
    *data_out = data;
    *size_out = size;
    return 0;
}

int swipterm_root_builder_list(swipterm_root_builder *b, const char *path, char ***names_out, size_t *count_out) {
    *names_out = NULL;
    *count_out = 0;
    if (b->failed || b->finished) {
        return -LINUX_EINVAL;
    }
    vfs_node_t *node = vfs_find(b->root, path, true);
    if (!node) {
        return -LINUX_ENOENT;
    }
    if (!S_ISDIR(node->mode)) {
        return -LINUX_ENOTDIR;
    }
    char **names = calloc(node->child_count > 0 ? node->child_count : 1, sizeof(*names));
    if (!names) {
        return -LINUX_ENOMEM;
    }
    for (size_t i = 0; i < node->child_count; i++) {
        names[i] = strdup(node->children[i].name);
        if (!names[i]) {
            swipterm_free_strings(names, i);
            return -LINUX_ENOMEM;
        }
    }
    *names_out = names;
    *count_out = node->child_count;
    return 0;
}

int swipterm_root_builder_write(swipterm_root_builder *b, const char *path, const void *data, size_t size,
                                uint32_t mode) {
    if (b->failed || b->finished) {
        return -LINUX_EINVAL;
    }
    vfs_node_t *parent = NULL;
    const char *leaf = NULL;
    size_t leaf_len = 0;
    if (!vfs_resolve_parent(b->root, &b->st, path, true, &parent, &leaf, &leaf_len) || !parent) {
        return -LINUX_EINVAL;
    }
    vfs_dirent_t *existing = vfs_dir_find(parent, leaf, leaf_len);
    if (existing && S_ISDIR(existing->node->mode)) {
        return -LINUX_EISDIR;
    }
    int rc = vfs_write_file(b->writer, parent, leaf, leaf_len, data, size, mode);
    if (rc < 0) {
        b->failed = true;
        return rc == -ENOMEM ? -LINUX_ENOMEM : rc == -ENOSPC ? -LINUX_ENOSPC : -LINUX_EIO;
    }
    return 0;
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

static bool stage_metadata(const char *path, char **error_message) {
    static const char json[] = "{\"version\":2,\"disk\":\"root.ext4\"}\n";
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        set_errno_error(error_message, "failed to write swipterm.json", errno);
        return false;
    }
    bool ok = write(fd, json, sizeof(json) - 1) == (ssize_t)(sizeof(json) - 1) && fsync(fd) == 0;
    int err = errno;
    ok = close(fd) == 0 && ok;
    if (!ok) {
        set_errno_error(error_message, "failed to write swipterm.json", err);
    }
    return ok;
}

static void remove_stale(const char *root_path) {
    static const char *const names[] = {"lower.erofs", "upper.ext4", ".preboot_snapshot"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char path[1024];
        if (snprintf(path, sizeof(path), "%s/%s", root_path, names[i]) < (int)sizeof(path)) {
            (void)unlink(path);
        }
    }
}

bool swipterm_root_builder_finish(swipterm_root_builder *b, char **error_message) {
    if (!usable(b, error_message)) {
        return false;
    }
    char *disk_path = NULL;
    char *metadata_partial_path = NULL;
    char *metadata_path = NULL;
    bool ok = asprintf(&disk_path, "%s/root.ext4", b->root_path) >= 0 &&
              asprintf(&metadata_partial_path, "%s/swipterm.json.partial", b->root_path) >= 0 &&
              asprintf(&metadata_path, "%s/swipterm.json", b->root_path) >= 0;
    if (!ok) {
        set_error(error_message, strerror(ENOMEM));
    }
    int rc = ok ? module_index_build(b->writer, b->root) : 0;
    if (rc < 0) {
        set_errno_error(error_message, "failed to index kernel modules", -rc);
        ok = false;
    }
    if (ok) {
        rc = ext4_writer_finish(b->writer, b->root);
        if (rc < 0) {
            set_errno_error(error_message, rc == -ENOSPC ? "the archive does not fit the disk size"
                                                         : "failed to write root disk", -rc);
            ok = false;
        }
    }
    if (ok && fsync(b->disk_fd) != 0) {
        set_errno_error(error_message, "failed to write root disk", errno);
        ok = false;
    }
    if (close(b->disk_fd) != 0 && ok) {
        set_errno_error(error_message, "failed to write root disk", errno);
        ok = false;
    }
    b->disk_fd = -1;
    ok = ok && stage_metadata(metadata_partial_path, error_message);
    if (ok && rename(b->partial_path, disk_path) != 0) {
        set_errno_error(error_message, "failed to install root disk", errno);
        ok = false;
    }
    if (ok && rename(metadata_partial_path, metadata_path) != 0) {
        set_errno_error(error_message, "failed to write swipterm.json", errno);
        ok = false;
    }
    if (ok) {
        remove_stale(b->root_path);
        (void)fsync(b->root_fd);
        (void)fsync_path(disk_path);
        close(b->root_fd);
        b->root_fd = -1;
        b->finished = true;
    } else {
        if (metadata_partial_path) {
            unlink(metadata_partial_path);
        }
        b->failed = true;
    }
    free(disk_path);
    free(metadata_partial_path);
    free(metadata_path);
    return ok;
}
