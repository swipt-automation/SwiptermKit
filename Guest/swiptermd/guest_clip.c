#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CLIPBOARD_PATH "/dev/clipboard"

typedef struct buffer {
    char *data;
    size_t len;
    size_t cap;
} buffer_t;

static const char *program;

static int fail(const char *subject) {
    fprintf(stderr, "%s: %s: %s\n", program, subject, strerror(errno));
    return 1;
}

static int usage(void) {
    printf("%s: Swipterm clipboard bridge for " CLIPBOARD_PATH "\n", program);
    return 0;
}

static int buffer_reserve(buffer_t *buffer, size_t extra) {
    if (buffer->cap - buffer->len >= extra)
        return 0;
    size_t cap = buffer->cap ? buffer->cap : 65536;
    while (cap - buffer->len < extra)
        cap *= 2;
    char *grown = realloc(buffer->data, cap);
    if (!grown)
        return -1;
    buffer->data = grown;
    buffer->cap = cap;
    return 0;
}

static int buffer_append(buffer_t *buffer, const char *data, size_t len) {
    if (buffer_reserve(buffer, len) < 0)
        return -1;
    memcpy(buffer->data + buffer->len, data, len);
    buffer->len += len;
    return 0;
}

static int buffer_read(buffer_t *buffer, int fd) {
    for (;;) {
        if (buffer_reserve(buffer, 16384) < 0)
            return -1;
        ssize_t n = read(fd, buffer->data + buffer->len, buffer->cap - buffer->len);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return (int)n;
        buffer->len += (size_t)n;
    }
}

static void trim_newline(buffer_t *buffer) {
    if (buffer->len > 0 && buffer->data[buffer->len - 1] == '\n')
        buffer->len--;
}

static int write_all(int fd, const char *data, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, data, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        data += n;
        len -= (size_t)n;
    }
    return 0;
}

static int read_stdin(buffer_t *buffer) {
    return buffer_read(buffer, STDIN_FILENO) < 0 ? fail("stdin") : 0;
}

static int emit(const buffer_t *buffer, bool newline) {
    if (write_all(STDOUT_FILENO, buffer->data, buffer->len) < 0 || (newline && write_all(STDOUT_FILENO, "\n", 1) < 0))
        return fail("stdout");
    return 0;
}

static int load(buffer_t *buffer) {
    int fd = open(CLIPBOARD_PATH, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return fail(CLIPBOARD_PATH);
    int rc = buffer_read(buffer, fd) < 0 ? fail(CLIPBOARD_PATH) : 0;
    close(fd);
    return rc;
}

static int store(const buffer_t *buffer) {
    int fd = open(CLIPBOARD_PATH, O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (fd < 0)
        return fail(CLIPBOARD_PATH);
    if (write_all(fd, buffer->data, buffer->len) < 0) {
        fail(CLIPBOARD_PATH);
        close(fd);
        return 1;
    }
    return close(fd) < 0 ? fail(CLIPBOARD_PATH) : 0;
}

static int list_types(const char *types) {
    if (access(CLIPBOARD_PATH, F_OK) < 0)
        return fail(CLIPBOARD_PATH);
    fputs(types, stdout);
    return 0;
}

static bool text_type(const char *type) {
    return strncmp(type, "text/", 5) == 0 || strcmp(type, "text") == 0 || strcmp(type, "TEXT") == 0 ||
           strcmp(type, "STRING") == 0 || strcmp(type, "UTF8_STRING") == 0;
}

static int pbcopy(int argc, char **argv) {
    (void)argc;
    (void)argv;
    buffer_t buffer = {0};
    return read_stdin(&buffer) || store(&buffer);
}

static int pbpaste(int argc, char **argv) {
    (void)argc;
    (void)argv;
    buffer_t buffer = {0};
    return load(&buffer) || emit(&buffer, false);
}

static int wl_copy(int argc, char **argv) {
    static const struct option options[] = {
        {"version", no_argument, NULL, 'v'},    {"help", no_argument, NULL, 'h'},
        {"primary", no_argument, NULL, 'p'},    {"trim-newline", no_argument, NULL, 'n'},
        {"paste-once", no_argument, NULL, 'o'}, {"foreground", no_argument, NULL, 'f'},
        {"clear", no_argument, NULL, 'c'},      {"type", required_argument, NULL, 't'},
        {"sensitive", no_argument, NULL, 'S'},  {"seat", required_argument, NULL, 's'},
        {NULL, 0, NULL, 0},
    };
    bool trim = false;
    bool clear = false;
    for (int c; (c = getopt_long(argc, argv, "vhpnofct:s:", options, NULL)) != -1;) {
        switch (c) {
        case 'n':
            trim = true;
            break;
        case 'c':
            clear = true;
            break;
        case 'v':
        case 'h':
            return usage();
        case '?':
            return 1;
        }
    }
    buffer_t buffer = {0};
    if (clear)
        return store(&buffer);
    if (optind == argc) {
        if (read_stdin(&buffer))
            return 1;
        if (trim)
            trim_newline(&buffer);
        return store(&buffer);
    }
    for (int i = optind; i < argc; i++) {
        if ((i > optind && buffer_append(&buffer, " ", 1) < 0) || buffer_append(&buffer, argv[i], strlen(argv[i])) < 0)
            return fail("argv");
    }
    return store(&buffer);
}

static int wl_paste(int argc, char **argv) {
    static const struct option options[] = {
        {"version", no_argument, NULL, 'v'},    {"help", no_argument, NULL, 'h'},
        {"primary", no_argument, NULL, 'p'},    {"no-newline", no_argument, NULL, 'n'},
        {"list-types", no_argument, NULL, 'l'}, {"watch", required_argument, NULL, 'w'},
        {"type", required_argument, NULL, 't'}, {"seat", required_argument, NULL, 's'},
        {NULL, 0, NULL, 0},
    };
    bool newline = true;
    bool list = false;
    const char *type = NULL;
    for (int c; (c = getopt_long(argc, argv, "vhpnlw:t:s:", options, NULL)) != -1;) {
        switch (c) {
        case 'n':
            newline = false;
            break;
        case 'l':
            list = true;
            break;
        case 't':
            type = optarg;
            break;
        case 'w':
            fprintf(stderr, "%s: --watch is not supported\n", program);
            return 1;
        case 'v':
        case 'h':
            return usage();
        case '?':
            return 1;
        }
    }
    if (list)
        return list_types("text/plain\ntext/plain;charset=utf-8\nUTF8_STRING\nSTRING\n");
    if (type && !text_type(type)) {
        fprintf(stderr, "No suitable type of content copied\n");
        return 1;
    }
    buffer_t buffer = {0};
    return load(&buffer) || emit(&buffer, newline);
}

static int xclip(int argc, char **argv) {
    enum { IGNORE, IN, OUT, TARGET, FILTER, RMLASTNL, HELP };
    static const struct {
        const char *name;
        int action;
        bool value;
    } options[] = {
        {"-loops", IGNORE, true},     {"-display", IGNORE, true},  {"-selection", IGNORE, true},
        {"-filter", FILTER, false},   {"-in", IN, false},          {"-out", OUT, false},
        {"-version", HELP, false},    {"-help", HELP, false},      {"-silent", IGNORE, false},
        {"-quiet", IGNORE, false},    {"-verbose", IGNORE, false}, {"-debug", IGNORE, false},
        {"-noutf8", IGNORE, false},   {"-target", TARGET, true},   {"-alt-text", IGNORE, true},
        {"-rmlastnl", RMLASTNL, false}, {"-sensitive", IGNORE, false}, {"-wait", IGNORE, true},
    };
    bool out = false;
    bool filter = false;
    bool trim = false;
    const char *target = NULL;
    int files = 0;
    for (int i = 1; i < argc; i++) {
        size_t len = strlen(argv[i]);
        int match = -1;
        for (int j = 0; len >= 2 && argv[i][0] == '-' && j < (int)(sizeof(options) / sizeof(options[0])); j++) {
            if (strncmp(argv[i], options[j].name, len) != 0)
                continue;
            if (options[j].name[len] == '\0') {
                match = j;
                break;
            }
            match = match == -1 ? j : -2;
        }
        if (match < 0 || (options[match].value && i + 1 == argc)) {
            argv[++files] = argv[i];
            continue;
        }
        const char *value = options[match].value ? argv[++i] : NULL;
        switch (options[match].action) {
        case IN:
            out = false;
            break;
        case OUT:
            out = true;
            break;
        case TARGET:
            target = value;
            break;
        case FILTER:
            filter = true;
            break;
        case RMLASTNL:
            trim = true;
            break;
        case HELP:
            return usage();
        }
    }
    buffer_t buffer = {0};
    if (out) {
        if (target && strcmp(target, "TARGETS") == 0)
            return list_types("TARGETS\nUTF8_STRING\nSTRING\ntext/plain\n");
        if (target && !text_type(target)) {
            fprintf(stderr, "Error: target %s not available\n", target);
            return 1;
        }
        if (load(&buffer))
            return 1;
        if (trim)
            trim_newline(&buffer);
        return emit(&buffer, false);
    }
    if (files == 0 && read_stdin(&buffer))
        return 1;
    for (int i = 1; i <= files; i++) {
        int fd = open(argv[i], O_RDONLY | O_CLOEXEC);
        if (fd < 0)
            return fail(argv[i]);
        int rc = buffer_read(&buffer, fd) < 0 ? fail(argv[i]) : 0;
        close(fd);
        if (rc)
            return rc;
    }
    if (filter && emit(&buffer, false))
        return 1;
    if (trim)
        trim_newline(&buffer);
    return store(&buffer);
}

static int xsel(int argc, char **argv) {
    enum { TRIM = 256, VERSION, DISPLAY, WINDOW_NAME };
    static const struct option options[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, VERSION},
        {"verbose", no_argument, NULL, 'v'},
        {"append", no_argument, NULL, 'a'},
        {"input", no_argument, NULL, 'i'},
        {"clear", no_argument, NULL, 'c'},
        {"output", no_argument, NULL, 'o'},
        {"follow", no_argument, NULL, 'f'},
        {"zeroflush", no_argument, NULL, 'z'},
        {"primary", no_argument, NULL, 'p'},
        {"secondary", no_argument, NULL, 's'},
        {"clipboard", no_argument, NULL, 'b'},
        {"trim", no_argument, NULL, TRIM},
        {"keep", no_argument, NULL, 'k'},
        {"exchange", no_argument, NULL, 'x'},
        {"display", required_argument, NULL, DISPLAY},
        {"windowName", required_argument, NULL, WINDOW_NAME},
        {"selectionTimeout", required_argument, NULL, 't'},
        {"name", required_argument, NULL, 'm'},
        {"nodetach", no_argument, NULL, 'n'},
        {"delete", no_argument, NULL, 'd'},
        {"logfile", required_argument, NULL, 'l'},
        {NULL, 0, NULL, 0},
    };
    bool input = !isatty(STDIN_FILENO);
    bool output = !isatty(STDOUT_FILENO) || !input;
    bool append = false;
    bool clear = false;
    bool trim = false;
    for (int c; (c = getopt_long(argc, argv, "hvaicofzpsbkxndt:m:l:", options, NULL)) != -1;) {
        switch (c) {
        case 'a':
            append = true;
            __attribute__((fallthrough));
        case 'i':
        case 'f':
        case 'z':
            input = true;
            output = false;
            break;
        case 'c':
        case 'd':
            clear = true;
            output = false;
            break;
        case 'o':
            input = false;
            output = true;
            break;
        case TRIM:
            trim = true;
            break;
        case 'k':
        case 'x':
            return 0;
        case 'h':
        case VERSION:
            return usage();
        case '?':
            return 1;
        }
    }
    if (output) {
        buffer_t current = {0};
        if (load(&current))
            return 1;
        if (trim)
            trim_newline(&current);
        if (emit(&current, false))
            return 1;
    }
    buffer_t buffer = {0};
    if (clear)
        return store(&buffer);
    if (!input)
        return 0;
    if ((append && load(&buffer)) || read_stdin(&buffer))
        return 1;
    if (trim)
        trim_newline(&buffer);
    return store(&buffer);
}

int main(int argc, char **argv) {
    static const struct {
        const char *name;
        int (*run)(int argc, char **argv);
    } tools[] = {
        {"pbcopy", pbcopy}, {"pbpaste", pbpaste}, {"wl-copy", wl_copy},
        {"wl-paste", wl_paste}, {"xclip", xclip}, {"xsel", xsel},
    };
    const char *slash = strrchr(argv[0], '/');
    program = slash ? slash + 1 : argv[0];
    for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
        if (strcmp(program, tools[i].name) == 0)
            return tools[i].run(argc, argv);
    }
    fprintf(stderr, "%s: run as pbcopy, pbpaste, wl-copy, wl-paste, xclip or xsel\n", program);
    return 1;
}
