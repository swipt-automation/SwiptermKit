#define _GNU_SOURCE

#include "guest_ipc.h"
#include "protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
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
    printf("%s: Swipterm clipboard bridge\n", program);
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

static int emit(const buffer_t *buffer, bool newline) {
    if (write_all_blocking(STDOUT_FILENO, buffer->data, buffer->len) < 0 ||
        (newline && write_all_blocking(STDOUT_FILENO, "\n", 1) < 0))
        return fail("stdout");
    return 0;
}

static bool text_type(const char *type) {
    return strncmp(type, "text/", 5) == 0 || strcmp(type, "text") == 0 || strcmp(type, "TEXT") == 0 ||
           strcmp(type, "STRING") == 0 || strcmp(type, "UTF8_STRING") == 0;
}

static const char *detect_mime(const buffer_t *buffer) {
    const unsigned char *b = (const unsigned char *)buffer->data;
    if (buffer->len >= 8 && memcmp(b, "\x89PNG\r\n\x1a\n", 8) == 0)
        return "image/png";
    if (buffer->len >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF)
        return "image/jpeg";
    if (buffer->len >= 6 && (memcmp(b, "GIF87a", 6) == 0 || memcmp(b, "GIF89a", 6) == 0))
        return "image/gif";
    if (buffer->len >= 12 && memcmp(b, "RIFF", 4) == 0 && memcmp(b + 8, "WEBP", 4) == 0)
        return "image/webp";
    return "text/plain;charset=utf-8";
}

static int connect_clip_socket(void) {
    struct sockaddr_un addr = {.sun_family = AF_UNIX, .sun_path = GUEST_CLIPBOARD_SOCKET};
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int load(buffer_t *buffer, bool primary, const char *mime, char *resolved_mime, size_t resolved_cap) {
    int sfd = connect_clip_socket();
    if (sfd >= 0) {
        size_t mlen = mime ? strlen(mime) : 0;
        if (mlen >= CLIP_MAX_MIME_LEN) {
            close(sfd);
            return -EINVAL;
        }
        uint8_t hdr[CLIP_SOCK_HEADER_SIZE] = {CLIP_SOCK_READ, primary ? 1 : 0};
        swiptermd_put_be16(hdr + 2, (uint16_t)mlen);
        uint8_t st_buf[4];
        uint8_t rlen_buf[2];
        int rc = write_all_blocking(sfd, hdr, sizeof(hdr));
        if (rc == 0)
            rc = write_all_blocking(sfd, mime, mlen);
        if (rc == 0)
            rc = read_all_blocking(sfd, st_buf, sizeof(st_buf));
        if (rc == 0)
            rc = (int32_t)swiptermd_get_be32(st_buf);
        if (rc == 0)
            rc = read_all_blocking(sfd, rlen_buf, sizeof(rlen_buf));
        if (rc < 0) {
            close(sfd);
            return rc;
        }
        uint16_t rlen = swiptermd_get_be16(rlen_buf);
        char rbuf[CLIP_MAX_MIME_LEN];
        if (rlen >= sizeof(rbuf) || read_all_blocking(sfd, rbuf, rlen) < 0) {
            close(sfd);
            return -EIO;
        }
        rbuf[rlen] = '\0';
        if (resolved_mime)
            snprintf(resolved_mime, resolved_cap, "%s", rbuf);
        rc = buffer_read(buffer, sfd) < 0 ? -errno : 0;
        close(sfd);
        return rc;
    }

    if (mime && !text_type(mime))
        return -EMEDIUMTYPE;
    int fd = open(CLIPBOARD_PATH, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    int rc = buffer_read(buffer, fd) < 0 ? -errno : 0;
    close(fd);
    if (rc == 0 && resolved_mime)
        snprintf(resolved_mime, resolved_cap, "text/plain;charset=utf-8");
    return rc;
}

static int store(const buffer_t *buffer, bool primary, const char *mime) {
    int sfd = connect_clip_socket();
    if (sfd >= 0) {
        size_t mlen = mime ? strlen(mime) : 0;
        if (mlen >= CLIP_MAX_MIME_LEN) {
            close(sfd);
            errno = EINVAL;
            return fail("mime");
        }
        uint8_t hdr[CLIP_SOCK_HEADER_SIZE] = {CLIP_SOCK_WRITE, primary ? 1 : 0};
        swiptermd_put_be16(hdr + 2, (uint16_t)mlen);
        swiptermd_put_be32(hdr + 4, (uint32_t)buffer->len);
        uint8_t reply[4];
        int rc = write_all_blocking(sfd, hdr, sizeof(hdr));
        if (rc == 0)
            rc = write_all_blocking(sfd, mime, mlen);
        if (rc == 0)
            rc = write_all_blocking(sfd, buffer->data, buffer->len);
        if (rc == 0)
            rc = read_all_blocking(sfd, reply, sizeof(reply));
        if (rc == 0)
            rc = (int32_t)swiptermd_get_be32(reply);
        close(sfd);
        if (rc < 0) {
            errno = -rc;
            return fail(GUEST_CLIPBOARD_SOCKET);
        }
        return 0;
    }

    int fd = open(CLIPBOARD_PATH, O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (fd < 0)
        return fail(CLIPBOARD_PATH);
    if (write_all_blocking(fd, buffer->data, buffer->len) < 0) {
        fail(CLIPBOARD_PATH);
        close(fd);
        return 1;
    }
    return close(fd) < 0 ? fail(CLIPBOARD_PATH) : 0;
}

static int list_types(bool primary, bool x11_targets) {
    int sfd = connect_clip_socket();
    if (sfd >= 0) {
        uint8_t hdr[CLIP_SOCK_HEADER_SIZE] = {CLIP_SOCK_LIST, primary ? 1 : 0};
        uint8_t cbuf[4];
        int rc = write_all_blocking(sfd, hdr, sizeof(hdr));
        if (rc == 0)
            rc = read_all_blocking(sfd, cbuf, sizeof(cbuf));
        if (rc < 0) {
            close(sfd);
            errno = -rc;
            return fail(GUEST_CLIPBOARD_SOCKET);
        }
        uint32_t count = swiptermd_get_be32(cbuf);
        if (count == 0) {
            close(sfd);
            if (!x11_targets) {
                fprintf(stderr, "Nothing is copied\n");
                return 1;
            }
            puts("TARGETS");
            return 0;
        }
        if (x11_targets)
            puts("TARGETS");
        for (uint32_t i = 0; i < count; i++) {
            uint8_t lbuf[4];
            if (read_all_blocking(sfd, lbuf, sizeof(lbuf)) < 0) {
                close(sfd);
                return 1;
            }
            uint32_t len = swiptermd_get_be32(lbuf);
            char mime[CLIP_MAX_MIME_LEN];
            if (len >= sizeof(mime) || read_all_blocking(sfd, mime, len) < 0) {
                close(sfd);
                return 1;
            }
            mime[len] = '\0';
            puts(mime);
        }
        close(sfd);
        return 0;
    }

    if (access(CLIPBOARD_PATH, F_OK) < 0)
        return fail(CLIPBOARD_PATH);
    if (x11_targets)
        fputs("TARGETS\nUTF8_STRING\nSTRING\ntext/plain\n", stdout);
    else
        fputs("text/plain\ntext/plain;charset=utf-8\nUTF8_STRING\nSTRING\n", stdout);
    return 0;
}

static int pbcopy(int argc, char **argv) {
    (void)argc;
    (void)argv;
    buffer_t buffer = {0};
    if (buffer_read(&buffer, STDIN_FILENO) < 0)
        return fail("stdin");
    return store(&buffer, false, detect_mime(&buffer));
}

static int pbpaste(int argc, char **argv) {
    (void)argc;
    (void)argv;
    buffer_t buffer = {0};
    int rc = load(&buffer, false, "text", NULL, 0);
    if (rc == -ENODATA || rc == -EMEDIUMTYPE)
        return 0;
    if (rc < 0) {
        errno = -rc;
        return fail("clipboard");
    }
    return emit(&buffer, false);
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
    bool primary = false;
    bool trim = false;
    bool clear = false;
    const char *type = NULL;
    for (int c; (c = getopt_long(argc, argv, "vhpnofct:s:", options, NULL)) != -1;) {
        switch (c) {
        case 'p':
            primary = true;
            break;
        case 'n':
            trim = true;
            break;
        case 'c':
            clear = true;
            break;
        case 't':
            type = optarg;
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
        return store(&buffer, primary, NULL);
    const char *mime = type;
    if (optind == argc) {
        if (buffer_read(&buffer, STDIN_FILENO) < 0)
            return fail("stdin");
        if (!mime)
            mime = detect_mime(&buffer);
    } else {
        for (int i = optind; i < argc; i++) {
            if ((i > optind && buffer_append(&buffer, " ", 1) < 0) ||
                buffer_append(&buffer, argv[i], strlen(argv[i])) < 0)
                return fail("argv");
        }
        if (!mime)
            mime = "text/plain;charset=utf-8";
    }
    if (trim && text_type(mime))
        trim_newline(&buffer);
    return store(&buffer, primary, mime);
}

static int wl_paste(int argc, char **argv) {
    static const struct option options[] = {
        {"version", no_argument, NULL, 'v'},    {"help", no_argument, NULL, 'h'},
        {"primary", no_argument, NULL, 'p'},    {"no-newline", no_argument, NULL, 'n'},
        {"list-types", no_argument, NULL, 'l'}, {"watch", required_argument, NULL, 'w'},
        {"type", required_argument, NULL, 't'}, {"seat", required_argument, NULL, 's'},
        {NULL, 0, NULL, 0},
    };
    bool primary = false;
    bool no_newline = false;
    bool list = false;
    const char *type = NULL;
    for (int c; (c = getopt_long(argc, argv, "vhpnlw:t:s:", options, NULL)) != -1;) {
        switch (c) {
        case 'p':
            primary = true;
            break;
        case 'n':
            no_newline = true;
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
        return list_types(primary, false);
    buffer_t buffer = {0};
    char resolved[CLIP_MAX_MIME_LEN] = {0};
    int rc = load(&buffer, primary, type, resolved, sizeof(resolved));
    if (rc == -ENODATA) {
        fprintf(stderr, "Nothing is copied\n");
        return 1;
    }
    if (rc == -EMEDIUMTYPE) {
        fprintf(stderr, "No suitable type of content copied\n");
        return 1;
    }
    if (rc < 0) {
        errno = -rc;
        return fail("clipboard");
    }
    return emit(&buffer, !no_newline && text_type(resolved));
}

static int xclip(int argc, char **argv) {
    enum { IGNORE, SELECTION, IN, OUT, TARGET, FILTER, RMLASTNL, HELP };
    static const struct {
        const char *name;
        int action;
        bool value;
    } options[] = {
        {"-loops", IGNORE, true},     {"-display", IGNORE, true},  {"-selection", SELECTION, true},
        {"-filter", FILTER, false},   {"-in", IN, false},          {"-out", OUT, false},
        {"-version", HELP, false},    {"-help", HELP, false},      {"-silent", IGNORE, false},
        {"-quiet", IGNORE, false},    {"-verbose", IGNORE, false}, {"-debug", IGNORE, false},
        {"-noutf8", IGNORE, false},   {"-target", TARGET, true},   {"-t", TARGET, true},
        {"-alt-text", IGNORE, true},  {"-rmlastnl", RMLASTNL, false}, {"-sensitive", IGNORE, false},
        {"-wait", IGNORE, true},
    };
    bool primary = false;
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
        case SELECTION:
            primary = value[0] == 'p';
            break;
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
            return list_types(primary, true);
        int rc = load(&buffer, primary, target ? target : "text", NULL, 0);
        if (rc < 0) {
            fprintf(stderr, "Error: target %s not available\n", target ? target : "STRING");
            return 1;
        }
        if (trim)
            trim_newline(&buffer);
        return emit(&buffer, false);
    }
    if (files == 0 && buffer_read(&buffer, STDIN_FILENO) < 0)
        return fail("stdin");
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
    const char *mime = target ? target : detect_mime(&buffer);
    if (trim && text_type(mime))
        trim_newline(&buffer);
    return store(&buffer, primary, mime);
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
    bool primary = false;
    bool input = !isatty(STDIN_FILENO);
    bool output = !isatty(STDOUT_FILENO) || !input;
    bool append = false;
    bool clear = false;
    bool trim = false;
    for (int c; (c = getopt_long(argc, argv, "hvaicofzpsbkxndt:m:l:", options, NULL)) != -1;) {
        switch (c) {
        case 'p':
            primary = true;
            break;
        case 'b':
            primary = false;
            break;
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
        int rc = load(&current, primary, "text", NULL, 0);
        if (rc < 0 && rc != -ENODATA && rc != -EMEDIUMTYPE) {
            errno = -rc;
            return fail("clipboard");
        }
        if (trim)
            trim_newline(&current);
        if (emit(&current, false))
            return 1;
    }
    buffer_t buffer = {0};
    if (clear)
        return store(&buffer, primary, NULL);
    if (!input)
        return 0;
    if (append) {
        int rc = load(&buffer, primary, "text", NULL, 0);
        if (rc < 0 && rc != -ENODATA && rc != -EMEDIUMTYPE) {
            errno = -rc;
            return fail("clipboard");
        }
    }
    if (buffer_read(&buffer, STDIN_FILENO) < 0)
        return fail("stdin");
    const char *mime = detect_mime(&buffer);
    if (trim && text_type(mime))
        trim_newline(&buffer);
    return store(&buffer, primary, mime);
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
