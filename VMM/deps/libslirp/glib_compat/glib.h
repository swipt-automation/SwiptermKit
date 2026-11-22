#ifndef VMM_DEPS_LIBSLIRP_GLIB_COMPAT_GLIB_H
#define VMM_DEPS_LIBSLIRP_GLIB_COMPAT_GLIB_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#define G_OS_UNIX 1
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define G_LITTLE_ENDIAN 1234
#define G_BIG_ENDIAN 4321
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define G_BYTE_ORDER G_BIG_ENDIAN
#define GUINT16_FROM_BE(x) ((uint16_t)(x))
#define GUINT16_TO_BE(x) ((uint16_t)(x))
#define GUINT32_FROM_BE(x) ((uint32_t)(x))
#define GUINT32_TO_BE(x) ((uint32_t)(x))
#define GUINT64_FROM_BE(x) ((uint64_t)(x))
#define GUINT64_TO_BE(x) ((uint64_t)(x))
#define GUINT16_FROM_LE(x) __builtin_bswap16((uint16_t)(x))
#define GUINT16_TO_LE(x) __builtin_bswap16((uint16_t)(x))
#define GUINT32_FROM_LE(x) __builtin_bswap32((uint32_t)(x))
#define GUINT32_TO_LE(x) __builtin_bswap32((uint32_t)(x))
#define GUINT64_FROM_LE(x) __builtin_bswap64((uint64_t)(x))
#define GUINT64_TO_LE(x) __builtin_bswap64((uint64_t)(x))
#else
#define G_BYTE_ORDER G_LITTLE_ENDIAN
#define GUINT16_FROM_BE(x) __builtin_bswap16((uint16_t)(x))
#define GUINT16_TO_BE(x) __builtin_bswap16((uint16_t)(x))
#define GUINT32_FROM_BE(x) __builtin_bswap32((uint32_t)(x))
#define GUINT32_TO_BE(x) __builtin_bswap32((uint32_t)(x))
#define GUINT64_FROM_BE(x) __builtin_bswap64((uint64_t)(x))
#define GUINT64_TO_BE(x) __builtin_bswap64((uint64_t)(x))
#define GUINT16_FROM_LE(x) ((uint16_t)(x))
#define GUINT16_TO_LE(x) ((uint16_t)(x))
#define GUINT32_FROM_LE(x) ((uint32_t)(x))
#define GUINT32_TO_LE(x) ((uint32_t)(x))
#define GUINT64_FROM_LE(x) ((uint64_t)(x))
#define GUINT64_TO_LE(x) ((uint64_t)(x))
#endif
#define GINT16_FROM_BE(x) ((int16_t)GUINT16_FROM_BE(x))
#define GINT16_TO_BE(x) ((int16_t)GUINT16_TO_BE(x))
#define GINT32_FROM_BE(x) ((int32_t)GUINT32_FROM_BE(x))
#define GINT32_TO_BE(x) ((int32_t)GUINT32_TO_BE(x))
#define GINT64_FROM_BE(x) ((int64_t)GUINT64_FROM_BE(x))
#define GINT64_TO_BE(x) ((int64_t)GUINT64_TO_BE(x))
#define GINT16_FROM_LE(x) ((int16_t)GUINT16_FROM_LE(x))
#define GINT16_TO_LE(x) ((int16_t)GUINT16_TO_LE(x))
#define GINT32_FROM_LE(x) ((int32_t)GUINT32_FROM_LE(x))
#define GINT32_TO_LE(x) ((int32_t)GUINT32_TO_LE(x))
#define GINT64_FROM_LE(x) ((int64_t)GUINT64_FROM_LE(x))
#define GINT64_TO_LE(x) ((int64_t)GUINT64_TO_LE(x))

#define GLIB_SIZEOF_VOID_P __SIZEOF_POINTER__
#define GLIB_CHECK_VERSION(major, minor, micro) \
    ((2 > (major)) || (2 == (major) && 68 > (minor)) || \
     (2 == (major) && 68 == (minor) && 0 >= (micro)))

#define G_GNUC_PRINTF(fmt, arg) __attribute__((format(printf, fmt, arg)))
#define G_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define G_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define G_N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))
#define G_STATIC_ASSERT(expr) _Static_assert((expr), #expr)
#define G_SIZEOF_MEMBER(type, member) sizeof(((type *)0)->member)
#define GLIB_UNUSED_PARAM(x) ((void)(x))

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

typedef int gboolean;
typedef char gchar;
typedef unsigned char guchar;
typedef int gint;
typedef unsigned int guint;
typedef short gshort;
typedef unsigned short gushort;
typedef long glong;
typedef unsigned long gulong;
typedef size_t gsize;
typedef ptrdiff_t gssize;
typedef int8_t gint8;
typedef uint8_t guint8;
typedef int16_t gint16;
typedef uint16_t guint16;
typedef int32_t gint32;
typedef uint32_t guint32;
typedef int64_t gint64;
typedef uint64_t guint64;
typedef void *gpointer;
typedef const void *gconstpointer;
typedef gchar **GStrv;
typedef int GPid;
typedef int GSpawnFlags;
typedef void (*GSpawnChildSetupFunc)(gpointer user_data);

#define G_SPAWN_SEARCH_PATH (1 << 2)

typedef struct GError {
    guint32 domain;
    gint code;
    gchar *message;
} GError;

typedef struct GDebugKey {
    const gchar *key;
    guint value;
} GDebugKey;

typedef struct GRand {
    uint32_t unused;
} GRand;

typedef struct GString {
    gchar *str;
    gsize len;
    gsize allocated_len;
} GString;

static inline gpointer g_malloc(gsize n_bytes) {
    if (n_bytes == 0) {
        return NULL;
    }
    void *p = malloc(n_bytes);
    if (!p) {
        abort();
    }
    return p;
}

static inline gpointer g_malloc0(gsize n_bytes) {
    if (n_bytes == 0) {
        return NULL;
    }
    void *p = calloc(1, n_bytes);
    if (!p) {
        abort();
    }
    return p;
}

static inline gpointer g_malloc_n(gsize n_blocks, gsize n_block_bytes) {
    if (n_blocks == 0 || n_block_bytes == 0) {
        return NULL;
    }
    if (n_blocks > SIZE_MAX / n_block_bytes) {
        abort();
    }
    return g_malloc(n_blocks * n_block_bytes);
}

static inline gpointer g_malloc0_n(gsize n_blocks, gsize n_block_bytes) {
    if (n_blocks == 0 || n_block_bytes == 0) {
        return NULL;
    }
    if (n_blocks > SIZE_MAX / n_block_bytes) {
        abort();
    }
    return g_malloc0(n_blocks * n_block_bytes);
}

static inline gpointer g_realloc(gpointer mem, gsize n_bytes) {
    if (n_bytes == 0) {
        free(mem);
        return NULL;
    }
    void *p = realloc(mem, n_bytes);
    if (!p) {
        abort();
    }
    return p;
}

static inline void g_free(gpointer mem) {
    free(mem);
}

#define g_new(struct_type, n_structs) \
    ((struct_type *)g_malloc_n((gsize)(n_structs), sizeof(struct_type)))
#define g_new0(struct_type, n_structs) \
    ((struct_type *)g_malloc0_n((gsize)(n_structs), sizeof(struct_type)))

static inline gchar *g_strdup(const gchar *str) {
    if (!str) {
        return NULL;
    }
    size_t len = strlen(str) + 1;
    gchar *copy = (gchar *)g_malloc(len);
    memcpy(copy, str, len);
    return copy;
}

static inline gchar *g_strndup(const gchar *str, gsize n) {
    if (!str) {
        return NULL;
    }
    size_t len = strnlen(str, n);
    gchar *copy = (gchar *)g_malloc(len + 1);
    memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

static inline gsize g_strlcpy(gchar *dest, const gchar *src, gsize dest_size) {
    return strlcpy(dest, src, dest_size);
}

static inline gboolean g_str_has_prefix(const gchar *str, const gchar *prefix) {
    if (!str || !prefix) {
        return FALSE;
    }
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

static inline gchar *g_strstr_len(const gchar *haystack, gssize haystack_len, const gchar *needle) {
    if (!haystack || !needle) {
        return NULL;
    }
    if (haystack_len < 0) {
        return (gchar *)strstr(haystack, needle);
    }
    size_t needle_len = strlen(needle);
    if (needle_len == 0) {
        return (gchar *)haystack;
    }
    size_t hlen = strnlen(haystack, (size_t)haystack_len);
    if (needle_len > hlen) {
        return NULL;
    }
    for (size_t i = 0; i <= hlen - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0) {
            return (gchar *)(haystack + i);
        }
    }
    return NULL;
}

static inline gint g_ascii_strcasecmp(const gchar *s1, const gchar *s2) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (*p1 && *p2) {
        int c1 = tolower(*p1);
        int c2 = tolower(*p2);
        if (c1 != c2) {
            return c1 - c2;
        }
        p1++;
        p2++;
    }
    return tolower(*p1) - tolower(*p2);
}

static inline void g_strfreev(gchar **str_array) {
    if (!str_array) {
        return;
    }
    for (size_t i = 0; str_array[i]; i++) {
        g_free(str_array[i]);
    }
    g_free(str_array);
}

static inline guint g_strv_length(gchar **str_array) {
    if (!str_array) {
        return 0;
    }
    guint n = 0;
    while (str_array[n]) {
        n++;
    }
    return n;
}

static inline gint g_vsnprintf(gchar *string, gulong n, const gchar *format, va_list args) {
    return vsnprintf(string, (size_t)n, format, args);
}

G_GNUC_PRINTF(3, 4)
static inline gint g_snprintf(gchar *string, gulong n, const gchar *format, ...) {
    va_list args;
    va_start(args, format);
    int r = vsnprintf(string, (size_t)n, format, args);
    va_end(args);
    return r;
}

static inline const gchar *g_strerror(gint errnum) {
    return strerror(errnum);
}

static inline const gchar *g_getenv(const gchar *variable) {
    return getenv(variable);
}

static inline GString *g_string_new(const gchar *init) {
    GString *s = g_new0(GString, 1);
    size_t init_len = init ? strlen(init) : 0;
    size_t cap = init_len + 64;
    s->str = (gchar *)g_malloc(cap);
    s->len = init_len;
    s->allocated_len = cap;
    if (init_len > 0) {
        memcpy(s->str, init, init_len);
    }
    s->str[init_len] = '\0';
    return s;
}

G_GNUC_PRINTF(2, 3)
static inline void g_string_append_printf(GString *string, const gchar *format, ...) {
    va_list args, args_copy;
    va_start(args, format);
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (needed < 0) {
        va_end(args_copy);
        return;
    }
    size_t req = string->len + (size_t)needed + 1;
    if (req > string->allocated_len) {
        size_t new_cap = string->allocated_len ? string->allocated_len * 2 : 64;
        while (new_cap < req) {
            new_cap *= 2;
        }
        string->str = (gchar *)g_realloc(string->str, new_cap);
        string->allocated_len = new_cap;
    }
    vsnprintf(string->str + string->len, string->allocated_len - string->len, format, args_copy);
    va_end(args_copy);
    string->len += (size_t)needed;
}

static inline gchar *g_string_free(GString *string, gboolean free_segment) {
    if (!string) {
        return NULL;
    }
    gchar *seg = string->str;
    g_free(string);
    if (free_segment) {
        g_free(seg);
        return NULL;
    }
    return seg;
}

static inline GRand *g_rand_new(void) {
    return g_new0(GRand, 1);
}

static inline void g_rand_free(GRand *rand_) {
    g_free(rand_);
}

static inline gint32 g_rand_int_range(GRand *rand_, gint32 begin, gint32 end) {
    (void)rand_;
    if (end <= begin) {
        return begin;
    }
    uint32_t span = (uint32_t)((int64_t)end - (int64_t)begin);
    return (gint32)((int64_t)begin + (int64_t)arc4random_uniform(span));
}

static inline guint g_parse_debug_string(const gchar *string, const GDebugKey *keys, guint nkeys) {
    if (!string) {
        return 0;
    }
    guint result = 0;
    const char *p = string;
    while (*p) {
        while (*p == ':' || *p == ',' || *p == ' ' || *p == '\t') {
            p++;
        }
        if (!*p) {
            break;
        }
        const char *start = p;
        while (*p && *p != ':' && *p != ',' && *p != ' ' && *p != '\t') {
            p++;
        }
        size_t len = (size_t)(p - start);
        if (len == 3 && strncmp(start, "all", 3) == 0) {
            for (guint i = 0; i < nkeys; i++) {
                result |= keys[i].value;
            }
        } else {
            for (guint i = 0; i < nkeys; i++) {
                if (strlen(keys[i].key) == len && strncmp(start, keys[i].key, len) == 0) {
                    result |= keys[i].value;
                }
            }
        }
    }
    return result;
}

static inline void g_error_free(GError *error) {
    if (!error) {
        return;
    }
    g_free(error->message);
    g_free(error);
}

static inline gboolean g_shell_parse_argv(const gchar *command_line, gint *argcp,
                                          gchar ***argvp, GError **error) {
    (void)command_line;
    if (argcp) {
        *argcp = 0;
    }
    if (argvp) {
        *argvp = NULL;
    }
    if (error) {
        GError *e = g_new0(GError, 1);
        e->code = ENOSYS;
        e->message = g_strdup("exec forwarding disabled");
        *error = e;
    }
    return FALSE;
}

static inline gboolean g_spawn_async_with_fds(const gchar *working_directory, gchar **argv,
                                              gchar **envp, GSpawnFlags flags,
                                              GSpawnChildSetupFunc child_setup, gpointer user_data,
                                              GPid *child_pid, gint stdin_fd, gint stdout_fd,
                                              gint stderr_fd, GError **error) {
    (void)working_directory;
    (void)argv;
    (void)envp;
    (void)flags;
    (void)child_setup;
    (void)user_data;
    (void)child_pid;
    (void)stdin_fd;
    (void)stdout_fd;
    (void)stderr_fd;
    if (error) {
        GError *e = g_new0(GError, 1);
        e->code = ENOSYS;
        e->message = g_strdup("exec forwarding disabled");
        *error = e;
    }
    return FALSE;
}

#define g_assert(expr) \
    do { \
        if (G_UNLIKELY(!(expr))) { \
            abort(); \
        } \
    } while (0)

#define g_assert_not_reached() abort()

#define g_warn_if_reached() ((void)0)
#define g_warn_if_fail(expr) ((void)(expr))

#define g_return_if_fail(expr) \
    do { \
        if (G_UNLIKELY(!(expr))) { \
            return; \
        } \
    } while (0)

#define g_return_val_if_fail(expr, val) \
    do { \
        if (G_UNLIKELY(!(expr))) { \
            return (val); \
        } \
    } while (0)

G_GNUC_PRINTF(1, 2)
static inline void g_slirp_log_noop(const gchar *fmt, ...) {
    (void)fmt;
}

G_GNUC_PRINTF(1, 2)
static inline void g_slirp_fatal(const gchar *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    abort();
}

#define g_debug(...) g_slirp_log_noop(__VA_ARGS__)
#define g_warning(...) g_slirp_log_noop(__VA_ARGS__)
#define g_warning_once(...) g_slirp_log_noop(__VA_ARGS__)
#define g_critical(...) g_slirp_log_noop(__VA_ARGS__)
#define g_error(...) g_slirp_fatal(__VA_ARGS__)

#endif
