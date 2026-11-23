#define _DARWIN_C_SOURCE

#include "host_internal.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    MODULE_FILE_LIMIT = 64 << 20,
    MODULE_NAME_MAX = 63,
    MODULE_TREE_DEPTH = 32,
};

static const char *const MODULE_BASES[] = {"/lib/modules", "/usr/lib/modules"};
static const char *const MODULE_SUFFIXES[] = {".ko", ".ko.gz", ".ko.xz", ".ko.zst"};

typedef struct module {
    char *path;
    char name[MODULE_NAME_MAX + 1];
    char *depends;
    char *softdeps;
    char *aliases;
    size_t *deps;
    size_t dep_count;
    uint8_t state;
} module_t;

typedef struct module_set {
    module_t *items;
    size_t count;
    size_t cap;
    size_t *order;
    size_t order_count;
} module_set_t;

typedef struct text {
    char *data;
    size_t len;
    size_t cap;
} text_t;

static int text_append(text_t *t, const char *s, size_t len) {
    if (t->len + len + 1 > t->cap) {
        size_t cap = t->cap ? t->cap * 2 : 65536;
        while (cap < t->len + len + 1) {
            cap *= 2;
        }
        char *data = realloc(t->data, cap);
        if (!data) {
            return -ENOMEM;
        }
        t->data = data;
        t->cap = cap;
    }
    memcpy(t->data + t->len, s, len);
    t->len += len;
    t->data[t->len] = '\0';
    return 0;
}

static int text_add(text_t *t, const char *s) {
    return text_append(t, s, strlen(s));
}

static void normalize(char *name) {
    for (char *p = name; *p; p++) {
        if (*p == '-') {
            *p = '_';
        }
    }
}

static size_t module_suffix(const char *name, size_t len) {
    for (size_t i = 0; i < sizeof(MODULE_SUFFIXES) / sizeof(MODULE_SUFFIXES[0]); i++) {
        size_t suffix = strlen(MODULE_SUFFIXES[i]);
        if (len > suffix && memcmp(name + len - suffix, MODULE_SUFFIXES[i], suffix) == 0) {
            return suffix;
        }
    }
    return 0;
}

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t le64(const uint8_t *p) {
    return (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32);
}

static bool find_modinfo(const uint8_t *elf, size_t len, const char **data_out, size_t *size_out) {
    if (len < 64 || memcmp(elf, "\177ELF", 4) != 0 || elf[4] != 2 || elf[5] != 1) {
        return false;
    }
    uint64_t shoff = le64(elf + 0x28);
    uint16_t shentsize = le16(elf + 0x3A);
    uint16_t shnum = le16(elf + 0x3C);
    uint16_t shstrndx = le16(elf + 0x3E);
    if (shentsize < 64 || shstrndx >= shnum || shoff > len || (uint64_t)shnum * shentsize > len - shoff) {
        return false;
    }
    const uint8_t *strtab = elf + shoff + (uint64_t)shstrndx * shentsize;
    uint64_t names = le64(strtab + 0x18);
    uint64_t names_size = le64(strtab + 0x20);
    if (names > len || names_size > len - names) {
        return false;
    }
    for (uint16_t i = 0; i < shnum; i++) {
        const uint8_t *sh = elf + shoff + (uint64_t)i * shentsize;
        uint32_t name = le32(sh);
        uint64_t offset = le64(sh + 0x18);
        uint64_t size = le64(sh + 0x20);
        if (name >= names_size || strncmp((const char *)elf + names + name, ".modinfo",
                                          (size_t)(names_size - name)) != 0) {
            continue;
        }
        if (offset > len || size > len - offset) {
            return false;
        }
        *data_out = (const char *)elf + offset;
        *size_out = (size_t)size;
        return true;
    }
    return false;
}

static int append_field(char **field, const char *separator, const char *value, size_t len) {
    size_t old = *field ? strlen(*field) : 0;
    size_t sep = *field ? strlen(separator) : 0;
    char *grown = realloc(*field, old + sep + len + 1);
    if (!grown) {
        return -ENOMEM;
    }
    memcpy(grown + old, separator, sep);
    memcpy(grown + old + sep, value, len);
    grown[old + sep + len] = '\0';
    *field = grown;
    return 0;
}

static int parse_modinfo(module_t *m, const char *info, size_t size) {
    int rc = 0;
    for (size_t pos = 0; pos < size && rc == 0;) {
        const char *entry = info + pos;
        size_t len = strnlen(entry, size - pos);
        pos += len + 1;
        const char *eq = memchr(entry, '=', len);
        if (!eq) {
            continue;
        }
        size_t key = (size_t)(eq - entry);
        const char *value = eq + 1;
        size_t value_len = len - key - 1;
        if (key == 4 && memcmp(entry, "name", 4) == 0 && value_len > 0 && value_len <= MODULE_NAME_MAX) {
            memcpy(m->name, value, value_len);
            m->name[value_len] = '\0';
        } else if (key == 7 && memcmp(entry, "depends", 7) == 0 && value_len > 0) {
            rc = append_field(&m->depends, ",", value, value_len);
        } else if (key == 7 && memcmp(entry, "softdep", 7) == 0 && value_len > 0) {
            rc = append_field(&m->softdeps, "\n", value, value_len);
        } else if (key == 5 && memcmp(entry, "alias", 5) == 0 && value_len > 0) {
            rc = append_field(&m->aliases, "\n", value, value_len);
        }
    }
    return rc;
}

static int add_module(ext4_writer_t *w, vfs_node_t *node, const char *path, const char *leaf, size_t suffix,
                      module_set_t *set) {
    if (node->size == 0 || node->size > MODULE_FILE_LIMIT) {
        return 0;
    }
    size_t raw_size = (size_t)node->size;
    uint8_t *raw = malloc(raw_size);
    if (!raw) {
        return -ENOMEM;
    }
    int rc = ext4_writer_read(w, node, 0, raw, raw_size);
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    if (rc == 0) {
        rc = swipterm_decompress(raw, raw_size, MODULE_FILE_LIMIT, &elf, &elf_size);
    }
    free(raw);
    if (rc == -ENOMEM) {
        return rc;
    }
    const char *info = NULL;
    size_t info_size = 0;
    if (rc < 0 || !find_modinfo(elf, elf_size, &info, &info_size)) {
        free(elf);
        return 0;
    }
    if (set->count == set->cap) {
        size_t cap = set->cap ? set->cap * 2 : 1024;
        module_t *items = realloc(set->items, cap * sizeof(*items));
        if (!items) {
            free(elf);
            return -ENOMEM;
        }
        set->items = items;
        set->cap = cap;
    }
    module_t *m = &set->items[set->count];
    *m = (module_t){.path = strdup(path)};
    size_t stem = strlen(leaf) - suffix;
    snprintf(m->name, sizeof(m->name), "%.*s", (int)(stem < MODULE_NAME_MAX ? stem : MODULE_NAME_MAX), leaf);
    rc = m->path ? parse_modinfo(m, info, info_size) : -ENOMEM;
    free(elf);
    normalize(m->name);
    set->count++;
    return rc;
}

static int collect_modules(ext4_writer_t *w, vfs_node_t *dir, char *path, size_t path_len, unsigned depth,
                           module_set_t *set) {
    for (size_t i = 0; i < dir->child_count; i++) {
        const vfs_dirent_t *de = &dir->children[i];
        if (path_len + de->name_len + 2 > PATH_MAX) {
            continue;
        }
        memcpy(path + path_len, de->name, de->name_len);
        path[path_len + de->name_len] = '\0';
        int rc = 0;
        if (S_ISDIR(de->node->mode) && depth < MODULE_TREE_DEPTH) {
            path[path_len + de->name_len] = '/';
            rc = collect_modules(w, de->node, path, path_len + de->name_len + 1, depth + 1, set);
        } else if (S_ISREG(de->node->mode)) {
            size_t suffix = module_suffix(de->name, de->name_len);
            if (suffix > 0) {
                rc = add_module(w, de->node, path, de->name, suffix, set);
            }
        }
        if (rc < 0) {
            return rc;
        }
    }
    return 0;
}

static int compare_names(const void *a, const void *b) {
    return strcmp(((const module_t *)a)->name, ((const module_t *)b)->name);
}

static ssize_t find_module(const module_set_t *set, const char *name) {
    size_t lo = 0;
    size_t hi = set->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = strcmp(set->items[mid].name, name);
        if (c == 0) {
            return (ssize_t)mid;
        }
        if (c < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return -1;
}

static int link_dependencies(module_set_t *set) {
    for (size_t i = 0; i < set->count; i++) {
        module_t *m = &set->items[i];
        if (!m->depends) {
            continue;
        }
        size_t count = 1;
        for (const char *p = m->depends; *p; p++) {
            count += *p == ',';
        }
        m->deps = malloc(count * sizeof(*m->deps));
        if (!m->deps) {
            return -ENOMEM;
        }
        char *save = NULL;
        for (char *name = strtok_r(m->depends, ",", &save); name; name = strtok_r(NULL, ",", &save)) {
            normalize(name);
            ssize_t index = find_module(set, name);
            if (index >= 0 && (size_t)index != i) {
                m->deps[m->dep_count++] = (size_t)index;
            }
        }
    }
    return 0;
}

static void visit(module_set_t *set, size_t index, unsigned depth) {
    module_t *m = &set->items[index];
    if (m->state != 0 || depth > MODULE_TREE_DEPTH) {
        return;
    }
    m->state = 1;
    for (size_t i = 0; i < m->dep_count; i++) {
        visit(set, m->deps[i], depth + 1);
    }
    set->order[set->order_count++] = index;
}

static int write_dep(module_set_t *set, text_t *out) {
    set->order = malloc((set->count > 0 ? set->count : 1) * sizeof(*set->order));
    if (!set->order) {
        return -ENOMEM;
    }
    int rc = 0;
    for (size_t i = 0; i < set->count && rc == 0; i++) {
        for (size_t k = 0; k < set->count; k++) {
            set->items[k].state = 0;
        }
        set->order_count = 0;
        visit(set, i, 0);
        rc = text_add(out, set->items[i].path);
        rc = rc == 0 ? text_add(out, ":") : rc;
        for (size_t k = set->order_count - 1; k > 0 && rc == 0; k--) {
            rc = text_add(out, " ");
            rc = rc == 0 ? text_add(out, set->items[set->order[k - 1]].path) : rc;
        }
        rc = rc == 0 ? text_add(out, "\n") : rc;
    }
    return rc;
}

static int write_lines(const module_set_t *set, bool aliases, text_t *out) {
    int rc = text_add(out, aliases ? "# Aliases extracted from modules themselves.\n"
                                   : "# Soft dependencies extracted from modules themselves.\n");
    for (size_t i = 0; i < set->count && rc == 0; i++) {
        const module_t *m = &set->items[i];
        const char *values = aliases ? m->aliases : m->softdeps;
        for (const char *line = values; line && *line && rc == 0;) {
            size_t len = strcspn(line, "\n");
            rc = text_add(out, aliases ? "alias " : "softdep ");
            if (aliases) {
                rc = rc == 0 ? text_append(out, line, len) : rc;
                rc = rc == 0 ? text_add(out, " ") : rc;
                rc = rc == 0 ? text_add(out, m->name) : rc;
            } else {
                rc = rc == 0 ? text_add(out, m->name) : rc;
                rc = rc == 0 ? text_add(out, " ") : rc;
                rc = rc == 0 ? text_append(out, line, len) : rc;
            }
            rc = rc == 0 ? text_add(out, "\n") : rc;
            line += len + (line[len] == '\n');
        }
    }
    return rc;
}

static void free_set(module_set_t *set) {
    for (size_t i = 0; i < set->count; i++) {
        free(set->items[i].path);
        free(set->items[i].depends);
        free(set->items[i].softdeps);
        free(set->items[i].aliases);
        free(set->items[i].deps);
    }
    free(set->items);
    free(set->order);
    *set = (module_set_t){0};
}

static int dep_matches_modules(ext4_writer_t *w, vfs_node_t *root, const char *dir_path, const vfs_node_t *dep,
                               bool *matches) {
    *matches = false;
    if (!S_ISREG(dep->mode) || dep->size > MODULE_FILE_LIMIT) {
        return 0;
    }
    size_t size = (size_t)dep->size;
    char *data = malloc(size + 1);
    char *path = malloc(PATH_MAX);
    int rc = data && path ? ext4_writer_read(w, dep, 0, data, size) : -ENOMEM;
    if (rc == 0) {
        data[size] = '\0';
        *matches = true;
        for (char *line = data; *matches && *line;) {
            size_t len = strcspn(line, "\n");
            const char *colon = memchr(line, ':', len);
            if (colon) {
                int name_len = (int)(colon - line);
                int n = line[0] == '/' ? snprintf(path, PATH_MAX, "%.*s", name_len, line)
                                       : snprintf(path, PATH_MAX, "%s/%.*s", dir_path, name_len, line);
                vfs_node_t *module = n < PATH_MAX ? vfs_find(root, path, true) : NULL;
                *matches = module && S_ISREG(module->mode);
            }
            line += len + (line[len] == '\n');
        }
    }
    free(data);
    free(path);
    return rc;
}

static int index_release(ext4_writer_t *w, vfs_node_t *root, vfs_node_t *dir, const char *dir_path) {
    vfs_dirent_t *dep = vfs_dir_find(dir, "modules.dep", 11);
    if (dep) {
        bool matches = false;
        int rc = dep_matches_modules(w, root, dir_path, dep->node, &matches);
        if (rc < 0 || matches) {
            return rc;
        }
    }
    module_set_t set = {0};
    char *path = malloc(PATH_MAX);
    int rc = path ? collect_modules(w, dir, path, 0, 0, &set) : -ENOMEM;
    free(path);
    if (rc == 0 && set.count == 0 && !vfs_dir_find(dir, "modules.builtin", 15)) {
        free_set(&set);
        return 0;
    }
    if (rc == 0) {
        qsort(set.items, set.count, sizeof(*set.items), compare_names);
        rc = link_dependencies(&set);
    }
    static const char *const stale_bins[] = {"modules.dep.bin", "modules.alias.bin", "modules.softdep.bin",
                                             "modules.symbols.bin"};
    for (size_t i = 0; i < 4; i++) {
        (void)vfs_dir_remove(dir, stale_bins[i], strlen(stale_bins[i]));
    }
    static const char *const names[] = {"modules.dep", "modules.softdep", "modules.alias"};
    for (size_t i = 0; i < 3 && rc == 0; i++) {
        text_t out = {0};
        rc = i == 0 ? write_dep(&set, &out) : write_lines(&set, i == 2, &out);
        if (rc == 0) {
            rc = vfs_write_file(w, dir, names[i], strlen(names[i]), out.data, out.len, 0644);
        }
        free(out.data);
    }
    free_set(&set);
    return rc;
}

int module_index_build(ext4_writer_t *w, vfs_node_t *root) {
    vfs_node_t *seen[2] = {NULL, NULL};
    for (size_t b = 0; b < 2; b++) {
        vfs_node_t *base = vfs_find(root, MODULE_BASES[b], true);
        if (!base || !S_ISDIR(base->mode) || base == seen[0]) {
            continue;
        }
        seen[b] = base;
        for (size_t i = 0; i < base->child_count; i++) {
            const vfs_dirent_t *release = &base->children[i];
            if (!S_ISDIR(release->node->mode)) {
                continue;
            }
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", MODULE_BASES[b], release->name);
            int rc = index_release(w, root, release->node, path);
            if (rc < 0) {
                return rc;
            }
        }
    }
    return 0;
}
