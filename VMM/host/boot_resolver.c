#define _DARWIN_C_SOURCE

#include "host_internal.h"

#include <CommonCrypto/CommonDigest.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
    RELEASE_MAX = 64,
    MODULE_NAME_MAX = 63,
    INDEX_FILE_LIMIT = 64 << 20,
    MODULE_FILE_LIMIT = 64 << 20,
    KERNEL_FILE_LIMIT = 256 << 20,
    KERNEL_PROBE_PREFIX = 1 << 20,
    KERNEL_PROBE_LIMIT = 32 << 20,
    VISIT_MAX_DEPTH = 64,
    MAX_BOOT_MODULES = 999,
};

static const char *const NIX_SYSTEM = "/nix/var/nix/profiles/system";
static const char *const MODULE_BASES[] = {"/lib/modules", "/usr/lib/modules"};
static const char *const INDEX_FILES[] = {"modules.dep", "modules.builtin", "modules.softdep", "modules.alias"};

typedef struct boot_requirement {
    const char *name;
    bool required;
} boot_requirement_t;

static const char GPU_MODULE[] = "virtio_gpu";

static const boot_requirement_t BOOT_MODULES[] = {
    {"crc32c_generic", false},
    {"crc32c", false},
    {"virtio_mmio", true},
    {"virtio_blk", true},
    {"ext4", true},
    {"vmw_vsock_virtio_transport", true},
    {"virtio_net", false},
    {"ipv6", false},
    {"virtio_rng", false},
    {"virtio_balloon", false},
    {"virtiofs", false},
    {"cuse", false},
    {"rtc_pl031", false},
};

typedef struct kernel_candidate {
    char release[RELEASE_MAX + 1];
    char *moddir;
    char *image_path;
    ext4_node_t image;
} kernel_candidate_t;

typedef struct boot_module {
    char name[MODULE_NAME_MAX + 1];
    char *path;
    ext4_node_t node;
    bool required;
} boot_module_t;

struct boot_plan {
    ext4_fs_t *fs;
    swipterm_kernel_selection selection;
    bool gpu;
    bool gpu_driver;
    char *wanted_release;

    kernel_candidate_t *candidates;
    size_t candidate_count;
    size_t next_candidate;

    const kernel_candidate_t *current;
    boot_module_t *modules;
    size_t module_count;
    uint8_t identity[CC_SHA256_DIGEST_LENGTH];
    char release[RELEASE_MAX + 1];
    char reasons[2048];
};

static char *path_join(const char *a, const char *b) {
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    char *out = malloc(alen + 1 + blen + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, a, alen);
    out[alen] = '/';
    memcpy(out + alen + 1, b, blen + 1);
    return out;
}

static void record_reason(boot_plan_t *plan, const char *fmt, ...) {
    size_t len = strlen(plan->reasons);
    if (len > 0 && len + 1 < sizeof(plan->reasons)) {
        plan->reasons[len++] = '\n';
        plan->reasons[len] = '\0';
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(plan->reasons + len, sizeof(plan->reasons) - len, fmt, ap);
    va_end(ap);
}

static int resolve_kind(ext4_fs_t *fs, const char *path, uint32_t kind, ext4_node_t *node) {
    int rc = ext4_fs_resolve(fs, path, true, node);
    if (rc == 0 && (node->mode & S_IFMT) != kind) {
        rc = kind == S_IFDIR ? -ENOTDIR : -EINVAL;
    }
    return rc;
}

static int compare_versions(const char *a, const char *b) {
    while (*a || *b) {
        if (*a == '~' || *b == '~') {
            if (*a != *b) {
                return *a == '~' ? -1 : 1;
            }
            a++;
            b++;
            continue;
        }
        bool da = *a >= '0' && *a <= '9';
        bool db = *b >= '0' && *b <= '9';
        if (da && db) {
            while (*a == '0') {
                a++;
            }
            while (*b == '0') {
                b++;
            }
            size_t la = strspn(a, "0123456789");
            size_t lb = strspn(b, "0123456789");
            if (la != lb) {
                return la < lb ? -1 : 1;
            }
            int c = memcmp(a, b, la);
            if (c != 0) {
                return c < 0 ? -1 : 1;
            }
            a += la;
            b += lb;
            continue;
        }
        if (da != db) {
            return da ? 1 : -1;
        }
        if (*a != *b) {
            return (unsigned char)*a < (unsigned char)*b ? -1 : 1;
        }
        a++;
        b++;
    }
    return 0;
}

static int compare_candidates_desc(const void *x, const void *y) {
    const kernel_candidate_t *a = x;
    const kernel_candidate_t *b = y;
    return -compare_versions(a->release, b->release);
}

typedef struct name_list {
    char (*names)[RELEASE_MAX + 1];
    size_t count;
    size_t cap;
} name_list_t;

static int collect_name(void *ctx, const char *name, size_t name_len, uint32_t ino) {
    (void)ino;
    name_list_t *l = ctx;
    if (name_len > RELEASE_MAX) {
        return 0;
    }
    if (l->count == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 8;
        char (*names)[RELEASE_MAX + 1] = realloc(l->names, cap * sizeof(*names));
        if (!names) {
            return -ENOMEM;
        }
        l->names = names;
        l->cap = cap;
    }
    memcpy(l->names[l->count], name, name_len);
    l->names[l->count][name_len] = '\0';
    l->count++;
    return 0;
}

static int add_candidate(boot_plan_t *plan, const char *release, const char *moddir, const char *image_path,
                         const ext4_node_t *image) {
    kernel_candidate_t *grown = realloc(plan->candidates, (plan->candidate_count + 1) * sizeof(*grown));
    if (!grown) {
        return -ENOMEM;
    }
    plan->candidates = grown;
    kernel_candidate_t *c = &plan->candidates[plan->candidate_count];
    *c = (kernel_candidate_t){.image = *image};
    snprintf(c->release, sizeof(c->release), "%s", release);
    c->moddir = strdup(moddir);
    c->image_path = strdup(image_path);
    if (!c->moddir || !c->image_path) {
        free(c->moddir);
        free(c->image_path);
        return -ENOMEM;
    }
    plan->candidate_count++;
    return 0;
}

static bool has_modules_dep(ext4_fs_t *fs, const char *moddir) {
    char *dep = path_join(moddir, "modules.dep");
    ext4_node_t node;
    bool ok = dep && resolve_kind(fs, dep, S_IFREG, &node) == 0;
    free(dep);
    return ok;
}

static int discover_nixos(boot_plan_t *plan) {
    ext4_node_t node;
    if (resolve_kind(plan->fs, NIX_SYSTEM, S_IFDIR, &node) != 0) {
        return 0;
    }
    char *base = path_join(NIX_SYSTEM, "kernel-modules/lib/modules");
    char *image_path = path_join(NIX_SYSTEM, "kernel");
    if (!base || !image_path) {
        free(base);
        free(image_path);
        return -ENOMEM;
    }
    name_list_t names = {0};
    ext4_node_t image;
    int rc = 0;
    if (resolve_kind(plan->fs, base, S_IFDIR, &node) == 0 &&
        resolve_kind(plan->fs, image_path, S_IFREG, &image) == 0) {
        rc = ext4_fs_list(plan->fs, &node, collect_name, &names);
    }
    for (size_t i = 0; i < names.count && rc == 0; i++) {
        char *moddir = path_join(base, names.names[i]);
        if (!moddir) {
            rc = -ENOMEM;
            break;
        }
        if (has_modules_dep(plan->fs, moddir)) {
            rc = add_candidate(plan, names.names[i], moddir, image_path, &image);
        }
        free(moddir);
    }
    free(names.names);
    free(base);
    free(image_path);
    return rc;
}

typedef struct module_dir {
    char release[RELEASE_MAX + 1];
    char *path;
    uint32_t ino;
} module_dir_t;

static int find_image(boot_plan_t *plan, const module_dir_t *dir, size_t dir_count, char **path_out,
                      ext4_node_t *image) {
    const char *release = dir->release;
    const char *dash = strrchr(release, '-');
    char paths[9][PATH_MAX];
    size_t n = 0;
    snprintf(paths[n++], PATH_MAX, "%s/vmlinuz", dir->path);
    snprintf(paths[n++], PATH_MAX, "%s/Image", dir->path);
    snprintf(paths[n++], PATH_MAX, "/boot/vmlinuz-%s", release);
    snprintf(paths[n++], PATH_MAX, "/boot/vmlinux-%s", release);
    snprintf(paths[n++], PATH_MAX, "/boot/Image-%s", release);
    snprintf(paths[n++], PATH_MAX, "/boot/kernel-%s", release);
    if (dash && dash[1] != '\0') {
        snprintf(paths[n++], PATH_MAX, "/boot/vmlinuz-%s", dash + 1);
    }
    if (dir_count == 1) {
        snprintf(paths[n++], PATH_MAX, "/boot/Image.gz");
        snprintf(paths[n++], PATH_MAX, "/boot/Image");
    }
    for (size_t i = 0; i < n; i++) {
        if (resolve_kind(plan->fs, paths[i], S_IFREG, image) == 0) {
            *path_out = strdup(paths[i]);
            return *path_out ? 0 : -ENOMEM;
        }
    }
    return -ENOENT;
}

static int discover_module_dirs(boot_plan_t *plan) {
    module_dir_t *dirs = NULL;
    size_t dir_count = 0;
    uint32_t base_inos[2] = {0};
    int rc = 0;
    for (size_t b = 0; b < 2 && rc == 0; b++) {
        ext4_node_t base;
        if (resolve_kind(plan->fs, MODULE_BASES[b], S_IFDIR, &base) != 0 ||
            (b == 1 && base.ino == base_inos[0])) {
            continue;
        }
        base_inos[b] = base.ino;
        name_list_t names = {0};
        rc = ext4_fs_list(plan->fs, &base, collect_name, &names);
        for (size_t i = 0; i < names.count && rc == 0; i++) {
            char *path = path_join(MODULE_BASES[b], names.names[i]);
            ext4_node_t node;
            if (!path) {
                rc = -ENOMEM;
                break;
            }
            bool duplicate = false;
            if (resolve_kind(plan->fs, path, S_IFDIR, &node) == 0 && has_modules_dep(plan->fs, path)) {
                for (size_t k = 0; k < dir_count; k++) {
                    duplicate = duplicate || dirs[k].ino == node.ino;
                }
                if (!duplicate) {
                    module_dir_t *grown = realloc(dirs, (dir_count + 1) * sizeof(*grown));
                    if (!grown) {
                        free(path);
                        rc = -ENOMEM;
                        break;
                    }
                    dirs = grown;
                    dirs[dir_count] = (module_dir_t){.path = path, .ino = node.ino};
                    snprintf(dirs[dir_count].release, sizeof(dirs[dir_count].release), "%s", names.names[i]);
                    dir_count++;
                    path = NULL;
                }
            }
            free(path);
        }
        free(names.names);
    }
    for (size_t i = 0; i < dir_count && rc == 0; i++) {
        char *image_path = NULL;
        ext4_node_t image;
        if (find_image(plan, &dirs[i], dir_count, &image_path, &image) == 0) {
            rc = add_candidate(plan, dirs[i].release, dirs[i].path, image_path, &image);
        }
        free(image_path);
    }
    for (size_t i = 0; i < dir_count; i++) {
        free(dirs[i].path);
    }
    free(dirs);
    return rc;
}

static void normalize_name(const char *src, size_t len, char *out, size_t out_size) {
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

static bool module_name_from_path(const char *path, size_t len, char *out, size_t out_size) {
    const char *base = path;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') {
            base = path + i + 1;
        }
    }
    size_t base_len = len - (size_t)(base - path);
    static const char *const suffixes[] = {".ko", ".ko.gz", ".ko.xz", ".ko.zst"};
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        size_t sl = strlen(suffixes[i]);
        if (base_len > sl && memcmp(base + base_len - sl, suffixes[i], sl) == 0) {
            if (base_len - sl >= out_size) {
                return false;
            }
            normalize_name(base, base_len - sl, out, out_size);
            return true;
        }
    }
    return false;
}

typedef struct dep_entry {
    char name[MODULE_NAME_MAX + 1];
    const char *path;
    const char *deps;
    uint8_t state;
    bool required;
} dep_entry_t;

typedef struct name_index {
    uint32_t *slots;
    size_t cap;
} name_index_t;

static uint32_t name_hash(const char *s) {
    uint32_t h = 2166136261U;
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 16777619U;
    }
    return h;
}

typedef struct alias_entry {
    char pattern[128];
    char module[MODULE_NAME_MAX + 1];
} alias_entry_t;

typedef struct softdep_entry {
    char module[MODULE_NAME_MAX + 1];
    const char *pre;
} softdep_entry_t;

typedef struct module_tree {
    ext4_fs_t *fs;
    const char *moddir;
    ext4_node_t index_nodes[4];
    bool index_present[4];
    uint8_t *dep_data;
    uint8_t *builtin_data;
    uint8_t *softdep_data;
    uint8_t *alias_data;
    bool alias_loaded;
    dep_entry_t *deps;
    size_t dep_count;
    name_index_t dep_index;
    char (*builtin)[MODULE_NAME_MAX + 1];
    size_t builtin_count;
    name_index_t builtin_index;
    softdep_entry_t *softdeps;
    size_t softdep_count;
    alias_entry_t *aliases;
    size_t alias_count;
    size_t *order;
    size_t order_count;
    char missing[MODULE_NAME_MAX + 1];
} module_tree_t;

static int index_build(name_index_t *idx, size_t count, const char *(*name_at)(const void *, size_t),
                       const void *items) {
    size_t cap = 16;
    while (cap < count * 2) {
        cap *= 2;
    }
    idx->slots = calloc(cap, sizeof(*idx->slots));
    if (!idx->slots) {
        return -ENOMEM;
    }
    idx->cap = cap;
    for (size_t i = 0; i < count; i++) {
        size_t pos = name_hash(name_at(items, i)) & (cap - 1);
        while (idx->slots[pos] != 0) {
            pos = (pos + 1) & (cap - 1);
        }
        idx->slots[pos] = (uint32_t)(i + 1);
    }
    return 0;
}

static ssize_t index_find(const name_index_t *idx, const char *name, const char *(*name_at)(const void *, size_t),
                          const void *items) {
    if (idx->cap == 0) {
        return -1;
    }
    size_t pos = name_hash(name) & (idx->cap - 1);
    while (idx->slots[pos] != 0) {
        size_t i = idx->slots[pos] - 1;
        if (strcmp(name_at(items, i), name) == 0) {
            return (ssize_t)i;
        }
        pos = (pos + 1) & (idx->cap - 1);
    }
    return -1;
}

static const char *dep_name_at(const void *items, size_t i) {
    return ((const dep_entry_t *)items)[i].name;
}

static const char *builtin_name_at(const void *items, size_t i) {
    return ((const char (*)[MODULE_NAME_MAX + 1])items)[i];
}

static int read_index(module_tree_t *t, size_t which, bool required, uint8_t **data_out) {
    *data_out = NULL;
    char *path = path_join(t->moddir, INDEX_FILES[which]);
    if (!path) {
        return -ENOMEM;
    }
    int rc = resolve_kind(t->fs, path, S_IFREG, &t->index_nodes[which]);
    free(path);
    if (rc < 0) {
        return required ? rc : 0;
    }
    t->index_present[which] = true;
    size_t size = 0;
    uint8_t *raw = NULL;
    rc = ext4_fs_read(t->fs, &t->index_nodes[which], INDEX_FILE_LIMIT, &raw, &size);
    if (rc < 0) {
        return rc;
    }
    uint8_t *data = realloc(raw, size + 1);
    if (!data) {
        free(raw);
        return -ENOMEM;
    }
    data[size] = '\0';
    *data_out = data;
    return 0;
}

static char *next_line(char **cursor) {
    char *line = *cursor;
    if (!line || *line == '\0') {
        return NULL;
    }
    char *nl = strchr(line, '\n');
    if (nl) {
        *nl = '\0';
        *cursor = nl + 1;
    } else {
        *cursor = line + strlen(line);
    }
    return line;
}

static int parse_dep(module_tree_t *t) {
    char *cursor = (char *)t->dep_data;
    size_t cap = 0;
    for (char *line; (line = next_line(&cursor));) {
        char *colon = strchr(line, ':');
        if (!colon) {
            continue;
        }
        *colon = '\0';
        if (t->dep_count == cap) {
            cap = cap ? cap * 2 : 1024;
            dep_entry_t *grown = realloc(t->deps, cap * sizeof(*grown));
            if (!grown) {
                return -ENOMEM;
            }
            t->deps = grown;
        }
        dep_entry_t *e = &t->deps[t->dep_count];
        *e = (dep_entry_t){.path = line, .deps = colon + 1};
        if (module_name_from_path(line, strlen(line), e->name, sizeof(e->name))) {
            t->dep_count++;
        }
    }
    return index_build(&t->dep_index, t->dep_count, dep_name_at, t->deps);
}

static int parse_builtin(module_tree_t *t) {
    char *cursor = (char *)t->builtin_data;
    size_t cap = 0;
    for (char *line; (line = next_line(&cursor));) {
        if (t->builtin_count == cap) {
            cap = cap ? cap * 2 : 256;
            char (*grown)[MODULE_NAME_MAX + 1] = realloc(t->builtin, cap * sizeof(*grown));
            if (!grown) {
                return -ENOMEM;
            }
            t->builtin = grown;
        }
        if (module_name_from_path(line, strlen(line), t->builtin[t->builtin_count], MODULE_NAME_MAX + 1)) {
            t->builtin_count++;
        }
    }
    return index_build(&t->builtin_index, t->builtin_count, builtin_name_at, t->builtin);
}

static int parse_softdep(module_tree_t *t) {
    char *cursor = (char *)t->softdep_data;
    for (char *line; (line = next_line(&cursor));) {
        char *save = NULL;
        char *word = strtok_r(line, " \t", &save);
        char *module = word ? strtok_r(NULL, " \t", &save) : NULL;
        if (!word || strcmp(word, "softdep") != 0 || !module) {
            continue;
        }
        char *rest = save;
        char *pre = rest ? strstr(rest, "pre:") : NULL;
        if (!pre) {
            continue;
        }
        pre += 4;
        char *post = strstr(pre, "post:");
        if (post) {
            *post = '\0';
        }
        softdep_entry_t *grown = realloc(t->softdeps, (t->softdep_count + 1) * sizeof(*grown));
        if (!grown) {
            return -ENOMEM;
        }
        t->softdeps = grown;
        softdep_entry_t *e = &t->softdeps[t->softdep_count++];
        normalize_name(module, strlen(module), e->module, sizeof(e->module));
        e->pre = pre;
    }
    return 0;
}

static int load_aliases(module_tree_t *t) {
    if (t->alias_loaded) {
        return 0;
    }
    t->alias_loaded = true;
    int rc = read_index(t, 3, false, &t->alias_data);
    if (rc < 0 || !t->alias_data) {
        return rc;
    }
    char *cursor = (char *)t->alias_data;
    size_t cap = 0;
    for (char *line; (line = next_line(&cursor));) {
        char *save = NULL;
        char *word = strtok_r(line, " \t", &save);
        char *pattern = word ? strtok_r(NULL, " \t", &save) : NULL;
        char *module = pattern ? strtok_r(NULL, " \t", &save) : NULL;
        if (!module || strcmp(word, "alias") != 0 || strlen(pattern) >= sizeof(t->aliases[0].pattern)) {
            continue;
        }
        if (t->alias_count == cap) {
            cap = cap ? cap * 2 : 1024;
            alias_entry_t *grown = realloc(t->aliases, cap * sizeof(*grown));
            if (!grown) {
                return -ENOMEM;
            }
            t->aliases = grown;
        }
        alias_entry_t *e = &t->aliases[t->alias_count++];
        normalize_name(pattern, strlen(pattern), e->pattern, sizeof(e->pattern));
        normalize_name(module, strlen(module), e->module, sizeof(e->module));
    }
    return 0;
}

static void module_tree_free(module_tree_t *t) {
    free(t->dep_data);
    free(t->builtin_data);
    free(t->softdep_data);
    free(t->alias_data);
    free(t->deps);
    free(t->dep_index.slots);
    free(t->builtin);
    free(t->builtin_index.slots);
    free(t->softdeps);
    free(t->aliases);
    free(t->order);
}

static bool is_builtin(const module_tree_t *t, const char *name) {
    return index_find(&t->builtin_index, name, builtin_name_at, t->builtin) >= 0;
}

static int visit_module(module_tree_t *t, size_t index, bool required, unsigned depth);

static int visit_name(module_tree_t *t, const char *raw, size_t raw_len, bool required, unsigned depth) {
    char name[128];
    normalize_name(raw, raw_len, name, sizeof(name));
    if (is_builtin(t, name)) {
        return 0;
    }
    ssize_t idx = index_find(&t->dep_index, name, dep_name_at, t->deps);
    if (idx >= 0) {
        return visit_module(t, (size_t)idx, required, depth);
    }
    int rc = load_aliases(t);
    if (rc < 0) {
        return rc;
    }
    bool matched = false;
    for (size_t i = 0; i < t->alias_count; i++) {
        alias_entry_t *a = &t->aliases[i];
        if (strcmp(a->pattern, name) != 0 && fnmatch(a->pattern, name, 0) != 0) {
            continue;
        }
        if (is_builtin(t, a->module)) {
            matched = true;
            continue;
        }
        ssize_t target = index_find(&t->dep_index, a->module, dep_name_at, t->deps);
        if (target >= 0) {
            rc = visit_module(t, (size_t)target, required, depth);
            if (rc < 0) {
                return rc;
            }
            matched = true;
        }
    }
    if (!matched && required) {
        snprintf(t->missing, sizeof(t->missing), "%s", name);
        return -ENOENT;
    }
    return 0;
}

static int visit_word_list(module_tree_t *t, const char *list, bool required, bool paths, unsigned depth) {
    const char *p = list;
    while (*p) {
        p += strspn(p, " \t");
        size_t len = strcspn(p, " \t");
        if (len == 0) {
            break;
        }
        int rc;
        if (paths) {
            char name[MODULE_NAME_MAX + 1];
            if (!module_name_from_path(p, len, name, sizeof(name))) {
                p += len;
                continue;
            }
            rc = visit_name(t, name, strlen(name), required, depth);
        } else {
            rc = visit_name(t, p, len, required, depth);
        }
        if (rc < 0) {
            return rc;
        }
        p += len;
    }
    return 0;
}

static int visit_module(module_tree_t *t, size_t index, bool required, unsigned depth) {
    dep_entry_t *e = &t->deps[index];
    if (e->state == 2) {
        if (!required || e->required) {
            return 0;
        }
        e->required = true;
        return visit_word_list(t, e->deps, true, true, depth + 1);
    }
    if (e->state == 1 || depth > VISIT_MAX_DEPTH) {
        return 0;
    }
    e->state = 1;
    for (size_t i = 0; i < t->softdep_count; i++) {
        if (strcmp(t->softdeps[i].module, e->name) == 0) {
            int rc = visit_word_list(t, t->softdeps[i].pre, false, false, depth + 1);
            if (rc < 0) {
                return rc;
            }
        }
    }
    int rc = visit_word_list(t, e->deps, required, true, depth + 1);
    if (rc < 0) {
        return rc;
    }
    e->state = 2;
    e->required = required;
    t->order[t->order_count++] = index;
    return 0;
}

static void sha_node(CC_SHA256_CTX *ctx, const ext4_node_t *node) {
    uint64_t fields[5] = {node->ino, node->generation, node->size, (uint64_t)node->mtime_ns,
                          (uint64_t)node->ctime_ns};
    CC_SHA256_Update(ctx, fields, sizeof(fields));
}

static void sha_string(CC_SHA256_CTX *ctx, const char *s) {
    CC_SHA256_Update(ctx, s, (CC_LONG)strlen(s) + 1);
}

static void clear_modules(boot_plan_t *plan) {
    for (size_t i = 0; i < plan->module_count; i++) {
        free(plan->modules[i].path);
    }
    free(plan->modules);
    plan->modules = NULL;
    plan->module_count = 0;
}

static int resolve_modules(boot_plan_t *plan, const kernel_candidate_t *c, char *reason, size_t reason_size) {
    module_tree_t t = {.fs = plan->fs, .moddir = c->moddir};
    int rc = read_index(&t, 0, true, &t.dep_data);
    if (rc == 0) {
        rc = read_index(&t, 1, false, &t.builtin_data);
    }
    if (rc == 0) {
        rc = read_index(&t, 2, false, &t.softdep_data);
    }
    if (rc == 0) {
        rc = parse_dep(&t);
    }
    if (rc == 0 && t.builtin_data) {
        rc = parse_builtin(&t);
    }
    if (rc == 0 && t.softdep_data) {
        rc = parse_softdep(&t);
    }
    if (rc == 0) {
        t.order = malloc((t.dep_count > 0 ? t.dep_count : 1) * sizeof(*t.order));
        rc = t.order ? 0 : -ENOMEM;
    }
    for (size_t i = 0; i < sizeof(BOOT_MODULES) / sizeof(BOOT_MODULES[0]) && rc == 0; i++) {
        rc = visit_name(&t, BOOT_MODULES[i].name, strlen(BOOT_MODULES[i].name), BOOT_MODULES[i].required, 0);
    }
    plan->gpu_driver = false;
    if (rc == 0 && plan->gpu) {
        const size_t order_count = t.order_count;
        rc = visit_name(&t, GPU_MODULE, strlen(GPU_MODULE), false, 0);
        plan->gpu_driver = rc == 0 && (is_builtin(&t, GPU_MODULE) || t.order_count > order_count);
    }
    if (rc == -ENOENT && t.missing[0] != '\0') {
        snprintf(reason, reason_size, "kernel %s has no %s module", c->release, t.missing);
    } else if (rc < 0) {
        snprintf(reason, reason_size, "kernel %s modules cannot be read (%s)", c->release, strerror(-rc));
    } else if (t.order_count > MAX_BOOT_MODULES) {
        snprintf(reason, reason_size, "kernel %s needs too many boot modules", c->release);
        rc = -E2BIG;
    }

    clear_modules(plan);
    if (rc == 0) {
        plan->modules = calloc(t.order_count > 0 ? t.order_count : 1, sizeof(*plan->modules));
        rc = plan->modules ? 0 : -ENOMEM;
    }
    for (size_t i = 0; i < t.order_count && rc == 0; i++) {
        const dep_entry_t *e = &t.deps[t.order[i]];
        boot_module_t *m = &plan->modules[plan->module_count];
        snprintf(m->name, sizeof(m->name), "%s", e->name);
        m->required = e->required;
        m->path = e->path[0] == '/' ? strdup(e->path) : path_join(c->moddir, e->path);
        if (!m->path) {
            rc = -ENOMEM;
            break;
        }
        plan->module_count++;
        int frc = resolve_kind(plan->fs, m->path, S_IFREG, &m->node);
        if (frc < 0 && m->required) {
            snprintf(reason, reason_size, "kernel %s module %s is missing", c->release, m->name);
            rc = frc;
        } else if (frc < 0) {
            free(m->path);
            plan->module_count--;
        }
    }

    if (rc == 0) {
        CC_SHA256_CTX ctx;
        CC_SHA256_Init(&ctx);
        sha_string(&ctx, "installed");
        sha_string(&ctx, c->release);
        sha_string(&ctx, c->image_path);
        sha_node(&ctx, &c->image);
        for (size_t i = 0; i < 4; i++) {
            if (t.index_present[i]) {
                sha_string(&ctx, INDEX_FILES[i]);
                sha_node(&ctx, &t.index_nodes[i]);
            }
        }
        for (size_t i = 0; i < plan->module_count; i++) {
            const boot_module_t *m = &plan->modules[i];
            sha_string(&ctx, m->required ? "R" : "O");
            sha_string(&ctx, m->path);
            sha_node(&ctx, &m->node);
        }
        CC_SHA256_Final(plan->identity, &ctx);
    } else {
        clear_modules(plan);
    }
    module_tree_free(&t);
    return rc;
}

typedef struct prefix_buffer {
    uint8_t *data;
    size_t len;
    size_t cap;
} prefix_buffer_t;

static int prefix_collect(void *ctx, const void *data, size_t len) {
    prefix_buffer_t *b = ctx;
    size_t n = len < b->cap - b->len ? len : b->cap - b->len;
    memcpy(b->data + b->len, data, n);
    b->len += n;
    return b->len == b->cap ? 1 : 0;
}

static int probe_image(boot_plan_t *plan, const kernel_candidate_t *c) {
    size_t want = c->image.size < KERNEL_PROBE_PREFIX ? (size_t)c->image.size : KERNEL_PROBE_PREFIX;
    for (int attempt = 0; attempt < 2; attempt++) {
        prefix_buffer_t b = {.data = malloc(want > 0 ? want : 1), .cap = want};
        if (!b.data) {
            return -ENOMEM;
        }
        int rc = ext4_fs_stream(plan->fs, &c->image, 0, want, prefix_collect, &b);
        size_t needed = 0;
        if (rc == 0) {
            rc = swipterm_kernel_probe(b.data, b.len, &needed);
        }
        free(b.data);
        if (rc != -EAGAIN) {
            return rc;
        }
        if (needed > KERNEL_PROBE_LIMIT || needed > c->image.size) {
            return -ENOEXEC;
        }
        want = needed;
    }
    return -ENOEXEC;
}

static const char *image_error(int rc) {
    switch (rc) {
    case -ENOTSUP:
        return "is not a little-endian 4K-page arm64 kernel";
    case -ENOEXEC:
    case -EINVAL:
        return "is not a supported arm64 kernel image";
    default:
        return strerror(-rc);
    }
}

static int select_next(boot_plan_t *plan) {
    while (plan->next_candidate < plan->candidate_count) {
        const kernel_candidate_t *c = &plan->candidates[plan->next_candidate++];
        if (plan->selection == SWIPTERM_KERNEL_RELEASE && strcmp(c->release, plan->wanted_release) != 0) {
            continue;
        }
        char reason[256] = {0};
        int rc = probe_image(plan, c);
        if (rc < 0) {
            snprintf(reason, sizeof(reason), "kernel %s image %s %s", c->release, c->image_path, image_error(rc));
        } else {
            rc = resolve_modules(plan, c, reason, sizeof(reason));
        }
        if (rc == -ENOMEM) {
            return rc;
        }
        if (rc < 0) {
            record_reason(plan, "%s", reason);
            continue;
        }
        plan->current = c;
        snprintf(plan->release, sizeof(plan->release), "%s", c->release);
        return 0;
    }
    if (plan->selection == SWIPTERM_KERNEL_RELEASE) {
        record_reason(plan, "kernel %s is not installed", plan->wanted_release);
    }
    plan->current = NULL;
    return -ENOEXEC;
}

int boot_plan_resolve(int disk_fd, swipterm_kernel_selection selection, const char *release, bool gpu,
                      boot_plan_t **plan_out, char **error_message) {
    *plan_out = NULL;
    if (selection == SWIPTERM_KERNEL_RELEASE && (!release || release[0] == '\0')) {
        return -EINVAL;
    }
    boot_plan_t *plan = calloc(1, sizeof(*plan));
    if (!plan) {
        return -ENOMEM;
    }
    plan->selection = selection;
    plan->gpu = gpu;
    plan->wanted_release = release ? strdup(release) : NULL;
    int rc = release && !plan->wanted_release ? -ENOMEM : ext4_fs_open(disk_fd, &plan->fs);
    if (rc == 0) {
        rc = discover_nixos(plan);
    }
    if (rc == 0 && plan->candidate_count == 0) {
        rc = discover_module_dirs(plan);
    }
    if (rc < 0 && rc != -ENOMEM) {
        record_reason(plan, "the root disk cannot be read (%s)", strerror(-rc));
        rc = -ENOEXEC;
    } else if (rc == 0) {
        qsort(plan->candidates, plan->candidate_count, sizeof(*plan->candidates), compare_candidates_desc);
        rc = select_next(plan);
    }
    if (rc == -ENOEXEC && error_message) {
        *error_message = strdup(plan->reasons);
    }
    if (rc < 0) {
        boot_plan_free(plan);
        return rc;
    }
    *plan_out = plan;
    return 0;
}

int boot_plan_next(boot_plan_t *plan, const char *reason) {
    record_reason(plan, "kernel %s %s", plan->release, reason);
    return select_next(plan);
}

const char *boot_plan_release(const boot_plan_t *plan) {
    return plan->release;
}

bool boot_plan_gpu(const boot_plan_t *plan) {
    return plan->gpu_driver;
}

const char *boot_plan_reasons(const boot_plan_t *plan) {
    return plan->reasons;
}

const uint8_t *boot_plan_identity(const boot_plan_t *plan) {
    return plan->identity;
}

typedef struct cpio_writer {
    uint8_t *data;
    size_t len;
    size_t cap;
    uint32_t next_ino;
} cpio_writer_t;

static int cpio_reserve(cpio_writer_t *w, size_t extra) {
    if (w->len + extra <= w->cap) {
        return 0;
    }
    size_t cap = w->cap ? w->cap : 1 << 20;
    while (cap < w->len + extra) {
        cap *= 2;
    }
    uint8_t *data = realloc(w->data, cap);
    if (!data) {
        return -ENOMEM;
    }
    w->data = data;
    w->cap = cap;
    return 0;
}

static void cpio_pad(cpio_writer_t *w) {
    while (w->len & 3U) {
        w->data[w->len++] = 0;
    }
}

static int cpio_add(cpio_writer_t *w, const char *name, uint32_t mode, const void *data, size_t size) {
    size_t name_size = strlen(name) + 1;
    int rc = cpio_reserve(w, 110 + name_size + 3 + size + 3);
    if (rc < 0) {
        return rc;
    }
    char header[111];
    snprintf(header, sizeof(header), "070701%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X",
             w->next_ino++, mode, 0U, 0U, S_ISDIR(mode) ? 2U : 1U, 0U, (uint32_t)size, 0U, 0U, 0U, 0U,
             (uint32_t)name_size, 0U);
    memcpy(w->data + w->len, header, 110);
    w->len += 110;
    memcpy(w->data + w->len, name, name_size);
    w->len += name_size;
    cpio_pad(w);
    if (size > 0) {
        memcpy(w->data + w->len, data, size);
        w->len += size;
    }
    cpio_pad(w);
    return 0;
}

static int load_module_file(boot_plan_t *plan, const boot_module_t *m, uint8_t **data_out, size_t *size_out) {
    uint8_t *raw = NULL;
    size_t raw_size = 0;
    int rc = ext4_fs_read(plan->fs, &m->node, MODULE_FILE_LIMIT, &raw, &raw_size);
    if (rc < 0) {
        return rc;
    }
    uint8_t *data = NULL;
    size_t size = 0;
    rc = swipterm_decompress(raw, raw_size, MODULE_FILE_LIMIT, &data, &size);
    free(raw);
    if (rc < 0) {
        return rc;
    }
    if (size < 4 || memcmp(data, "\177ELF", 4) != 0) {
        free(data);
        return -ENOEXEC;
    }
    *data_out = data;
    *size_out = size;
    return 0;
}

static int build_modules_cpio(boot_plan_t *plan, uint8_t **cpio_out, size_t *cpio_size_out, char *reason,
                              size_t reason_size) {
    cpio_writer_t w = {.next_ino = 1};
    char *list = NULL;
    size_t list_len = 0;
    int rc = cpio_add(&w, "swipterm", S_IFDIR | 0755, NULL, 0);
    if (rc == 0) {
        rc = cpio_add(&w, "swipterm/modules", S_IFDIR | 0755, NULL, 0);
    }
    for (size_t i = 0; i < plan->module_count && rc == 0; i++) {
        const boot_module_t *m = &plan->modules[i];
        uint8_t *data = NULL;
        size_t size = 0;
        rc = load_module_file(plan, m, &data, &size);
        if (rc < 0) {
            if (!m->required) {
                rc = 0;
                continue;
            }
            snprintf(reason, reason_size, "module %s cannot be loaded (%s)", m->name,
                     rc == -ENOEXEC ? "not an ELF module" : strerror(-rc));
            break;
        }
        char file[MODULE_NAME_MAX + 32];
        snprintf(file, sizeof(file), "%03zu-%s.ko", i, m->name);
        char path[MODULE_NAME_MAX + 64];
        snprintf(path, sizeof(path), "swipterm/modules/%s", file);
        rc = cpio_add(&w, path, S_IFREG | 0644, data, size);
        free(data);
        char line[MODULE_NAME_MAX + 64];
        int line_len = snprintf(line, sizeof(line), "%c /%s\n", m->required ? 'R' : 'O', path);
        char *grown = rc == 0 ? realloc(list, list_len + (size_t)line_len + 1) : NULL;
        if (rc == 0 && !grown) {
            rc = -ENOMEM;
        }
        if (rc == 0) {
            list = grown;
            memcpy(list + list_len, line, (size_t)line_len + 1);
            list_len += (size_t)line_len;
        }
    }
    if (rc == 0) {
        rc = cpio_add(&w, "swipterm/modules.load", S_IFREG | 0644, list, list_len);
    }
    if (rc == 0) {
        rc = cpio_add(&w, "TRAILER!!!", 0, NULL, 0);
    }
    free(list);
    if (rc < 0) {
        free(w.data);
        return rc;
    }
    *cpio_out = w.data;
    *cpio_size_out = w.len;
    return 0;
}

static int load_installed(boot_plan_t *plan, uint8_t **kernel_out, size_t *kernel_size_out, char *reason,
                          size_t reason_size) {
    const kernel_candidate_t *c = plan->current;
    uint8_t *file = NULL;
    size_t size = 0;
    int rc = ext4_fs_read(plan->fs, &c->image, KERNEL_FILE_LIMIT, &file, &size);
    if (rc < 0) {
        snprintf(reason, reason_size, "image %s cannot be read (%s)", c->image_path, strerror(-rc));
        return rc;
    }
    uint8_t *image = NULL;
    size_t image_size = 0;
    rc = swipterm_kernel_image(file, size, &image, &image_size);
    free(file);
    if (rc < 0) {
        snprintf(reason, reason_size, "image %s %s", c->image_path, image_error(rc));
        return rc;
    }
    char banner[RELEASE_MAX + 32];
    int banner_len = snprintf(banner, sizeof(banner), "Linux version %s ", c->release);
    if (!memmem(image, image_size, banner, (size_t)banner_len)) {
        free(image);
        snprintf(reason, reason_size, "image %s is not built for this release", c->image_path);
        return -ESTALE;
    }
    *kernel_out = image;
    *kernel_size_out = image_size;
    return 0;
}

int boot_plan_load(boot_plan_t *plan, uint8_t **kernel_out, size_t *kernel_size_out,
                   uint8_t **modules_cpio_out, size_t *modules_cpio_size_out) {
    *kernel_out = NULL;
    *kernel_size_out = 0;
    *modules_cpio_out = NULL;
    *modules_cpio_size_out = 0;
    char reason[192] = {0};
    int rc = load_installed(plan, kernel_out, kernel_size_out, reason, sizeof(reason));
    if (rc == 0) {
        rc = build_modules_cpio(plan, modules_cpio_out, modules_cpio_size_out, reason, sizeof(reason));
        if (rc < 0) {
            free(*kernel_out);
            *kernel_out = NULL;
            *kernel_size_out = 0;
        }
    }
    if (rc == -ENOMEM) {
        return rc;
    }
    if (rc < 0) {
        rc = boot_plan_next(plan, reason);
        return rc < 0 ? rc : -EAGAIN;
    }
    return 0;
}

void boot_plan_free(boot_plan_t *plan) {
    if (!plan) {
        return;
    }
    clear_modules(plan);
    for (size_t i = 0; i < plan->candidate_count; i++) {
        free(plan->candidates[i].moddir);
        free(plan->candidates[i].image_path);
    }
    free(plan->candidates);
    ext4_fs_close(plan->fs);
    free(plan->wanted_release);
    free(plan);
}
