#define _DARWIN_C_SOURCE

#include "host_internal.h"
#include "host_util.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zstd.h>

enum : uint32_t {
    STREAM_BUF_SIZE = 1U << 20,
    XZ_TAIL_SIZE = 4096U,
    XZ_INDEX_MAX_SIZE = 4U << 20,
};

enum {
    CODEC_NONE,
    CODEC_XZ,
    CODEC_ZSTD,
};

static int input_fill(archive_stream_t *s, struct archive *a) {
    if (s->in_off < s->in_len || s->in_eof) {
        return 0;
    }
    for (;;) {
        ssize_t n = s->in.entry ? (ssize_t)archive_read_data(s->in.entry, s->in_buf, STREAM_BUF_SIZE)
                                : read(s->in.fd, s->in_buf, STREAM_BUF_SIZE);
        if (n < 0 && !s->in.entry && errno == EINTR) {
            continue;
        }
        if (n < 0) {
            if (s->in.entry) {
                archive_set_error(a, ARCHIVE_ERRNO_MISC, "%s", archive_error_string(s->in.entry));
            } else {
                archive_set_error(a, errno, "%s", strerror(errno));
            }
            return -1;
        }
        s->in_off = 0;
        s->in_len = (size_t)n;
        s->in_eof = n == 0;
        s->pos += (uint64_t)n;
        return 0;
    }
}

static int next_input(archive_stream_t *s, struct archive *a, const uint8_t **data_out, size_t *len_out) {
    if (input_fill(s, a) < 0) {
        return -1;
    }
    *data_out = s->in_buf + s->in_off;
    *len_out = s->in_len - s->in_off;
    s->in_off = s->in_len;
    return 0;
}

static la_ssize_t read_xz(struct archive *a, archive_stream_t *s, const void **buffer) {
    s->xz.next_out = s->out_buf;
    s->xz.avail_out = STREAM_BUF_SIZE;
    while (!s->end && s->xz.avail_out == STREAM_BUF_SIZE) {
        if (s->xz.avail_in == 0 && !s->xz_input_end) {
            const uint8_t *data = NULL;
            size_t len = 0;
            if (next_input(s, a, &data, &len) < 0) {
                return ARCHIVE_FATAL;
            }
            s->xz.next_in = data;
            s->xz.avail_in = len;
            s->xz_input_end = len == 0;
        }
        lzma_ret ret = lzma_code(&s->xz, s->xz_input_end ? LZMA_FINISH : LZMA_RUN);
        if (ret == LZMA_STREAM_END) {
            s->end = true;
        } else if (ret != LZMA_OK) {
            archive_set_error(a, ARCHIVE_ERRNO_MISC, "xz decompression failed (liblzma error %d)", (int)ret);
            return ARCHIVE_FATAL;
        }
    }
    *buffer = s->out_buf;
    return (la_ssize_t)(STREAM_BUF_SIZE - s->xz.avail_out);
}

static la_ssize_t read_zstd(struct archive *a, archive_stream_t *s, const void **buffer) {
    ZSTD_outBuffer out = {.dst = s->out_buf, .size = STREAM_BUF_SIZE, .pos = 0};
    while (out.pos == 0) {
        if (s->zstd_in.pos == s->zstd_in.size && !s->zstd_input_end) {
            const uint8_t *data = NULL;
            size_t len = 0;
            if (next_input(s, a, &data, &len) < 0) {
                return ARCHIVE_FATAL;
            }
            s->zstd_in = (ZSTD_inBuffer){.src = data, .size = len, .pos = 0};
            s->zstd_input_end = len == 0;
        }
        if (s->zstd_in.pos == s->zstd_in.size && s->zstd_input_end) {
            if (s->zstd_last != 0) {
                archive_set_error(a, ARCHIVE_ERRNO_MISC, "truncated zstd stream");
                return ARCHIVE_FATAL;
            }
            break;
        }
        size_t ret = ZSTD_decompressStream(s->zstd, &out, &s->zstd_in);
        if (ZSTD_isError(ret)) {
            archive_set_error(a, ARCHIVE_ERRNO_MISC, "zstd decompression failed: %s", ZSTD_getErrorName(ret));
            return ARCHIVE_FATAL;
        }
        s->zstd_last = ret;
    }
    *buffer = s->out_buf;
    return (la_ssize_t)out.pos;
}

static la_ssize_t stream_read(struct archive *a, void *client, const void **buffer) {
    archive_stream_t *s = client;
    switch (s->codec) {
    case CODEC_XZ:
        return read_xz(a, s, buffer);
    case CODEC_ZSTD:
        return read_zstd(a, s, buffer);
    default: {
        const uint8_t *data = NULL;
        size_t len = 0;
        if (next_input(s, a, &data, &len) < 0) {
            return ARCHIVE_FATAL;
        }
        *buffer = data;
        return (la_ssize_t)len;
    }
    }
}

static const uint8_t *tail_bytes(const archive_stream_t *s, uint64_t off, size_t len, uint8_t *buf) {
    return pread_all(s->in.fd, buf, len, (off_t)(s->in.offset + off)) == (ssize_t)len ? buf : NULL;
}

static uint64_t xz_uncompressed_size(const archive_stream_t *s) {
    uint64_t size = s->in.size;
    uint8_t tail_buf[XZ_TAIL_SIZE];
    size_t tail_len = size < XZ_TAIL_SIZE ? (size_t)size : XZ_TAIL_SIZE;
    const uint8_t *tail = tail_bytes(s, size - tail_len, tail_len, tail_buf);
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
    uint8_t *index_buf = malloc(index_size);
    if (!index_buf) {
        return 0;
    }
    const uint8_t *index_bytes = tail_bytes(s, index_end - index_size, index_size, index_buf);
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

int archive_stream_open(archive_stream_t *s, struct archive *a, const archive_input_t *input) {
    *s = (archive_stream_t){.in = *input};
    if (!s->in.entry && lseek(s->in.fd, (off_t)s->in.offset, SEEK_SET) < 0) {
        archive_set_error(a, errno, "%s", strerror(errno));
        return ARCHIVE_FATAL;
    }
    s->in_buf = malloc(STREAM_BUF_SIZE);
    if (!s->in_buf) {
        archive_set_error(a, ENOMEM, "%s", strerror(ENOMEM));
        return ARCHIVE_FATAL;
    }
    if (input_fill(s, a) < 0) {
        return ARCHIVE_FATAL;
    }
    const uint8_t *magic = s->in_buf;
    size_t magic_len = s->in_len;
    if (magic_len >= sizeof(XZ_MAGIC) && memcmp(magic, XZ_MAGIC, sizeof(XZ_MAGIC)) == 0) {
        s->codec = CODEC_XZ;
    } else if (magic_len >= 4 && get_le32(magic) == ZSTD_MAGICNUMBER) {
        s->codec = CODEC_ZSTD;
    }
    if (s->codec == CODEC_XZ) {
        uint32_t threads = lzma_cputhreads();
        lzma_mt mt = {
            .flags = LZMA_CONCATENATED,
            .threads = threads ? threads : 1U,
            .memlimit_threading = UINT64_MAX,
            .memlimit_stop = UINT64_MAX,
        };
        s->out_buf = malloc(STREAM_BUF_SIZE);
        if (!s->out_buf || lzma_stream_decoder_mt(&s->xz, &mt) != LZMA_OK) {
            archive_set_error(a, ENOMEM, "failed to initialize decompressor");
            return ARCHIVE_FATAL;
        }
        s->xz_open = true;
        if (!s->in.entry) {
            s->uncompressed = xz_uncompressed_size(s);
        }
    } else if (s->codec == CODEC_ZSTD) {
        s->zstd = ZSTD_createDCtx();
        s->out_buf = malloc(STREAM_BUF_SIZE);
        if (!s->zstd || !s->out_buf) {
            archive_set_error(a, ENOMEM, "failed to initialize decompressor");
            return ARCHIVE_FATAL;
        }
        (void)ZSTD_DCtx_setParameter(s->zstd, ZSTD_d_windowLogMax, 31);
    }
    int r = s->codec == CODEC_NONE ? archive_read_support_filter_all(a) : archive_read_support_filter_none(a);
    if (r == ARCHIVE_OK || r == ARCHIVE_WARN) {
        r = archive_read_open(a, s, NULL, stream_read, NULL);
    }
    return r == ARCHIVE_WARN ? ARCHIVE_OK : r;
}

double archive_stream_progress(struct archive *a, const archive_stream_t *s) {
    if (s->uncompressed > 0) {
        return (double)archive_filter_bytes(a, -1) / (double)s->uncompressed;
    }
    return s->in.size > 0 ? (double)s->pos / (double)s->in.size : 0.0;
}

void archive_stream_close(archive_stream_t *s) {
    if (s->xz_open) {
        lzma_end(&s->xz);
    }
    ZSTD_freeDCtx(s->zstd);
    free(s->in_buf);
    free(s->out_buf);
    *s = (archive_stream_t){0};
}

struct swipterm_archive {
    struct archive *archive;
    archive_stream_t stream;
    int fd;
};

void swipterm_archive_close(swipterm_archive *archive) {
    if (!archive) {
        return;
    }
    archive_read_free(archive->archive);
    archive_stream_close(&archive->stream);
    if (archive->fd >= 0) {
        close(archive->fd);
    }
    free(archive);
}

swipterm_archive *swipterm_archive_open(const char *path, char **error_message) {
    if (error_message) {
        *error_message = NULL;
    }
    swipterm_archive *archive = calloc(1, sizeof(*archive));
    if (!archive) {
        set_error(error_message, strerror(ENOMEM));
        return NULL;
    }
    archive->fd = open(path, O_RDONLY | O_CLOEXEC);
    struct stat st;
    if (archive->fd < 0 || fstat(archive->fd, &st) != 0) {
        set_error(error_message, strerror(errno));
        swipterm_archive_close(archive);
        return NULL;
    }
    archive->archive = archive_read_new();
    if (!archive->archive) {
        set_error(error_message, strerror(ENOMEM));
        swipterm_archive_close(archive);
        return NULL;
    }
    archive_input_t input = {.fd = archive->fd, .size = (uint64_t)st.st_size};
    if (archive_read_support_format_tar(archive->archive) != ARCHIVE_OK ||
        archive_read_support_format_raw(archive->archive) != ARCHIVE_OK ||
        archive_stream_open(&archive->stream, archive->archive, &input) != ARCHIVE_OK) {
        set_error(error_message, archive_error_string(archive->archive));
        swipterm_archive_close(archive);
        return NULL;
    }
    return archive;
}

int swipterm_archive_next(swipterm_archive *archive, const char **name_out, char **error_message) {
    if (error_message) {
        *error_message = NULL;
    }
    struct archive_entry *entry = NULL;
    for (;;) {
        int r = archive_read_next_header(archive->archive, &entry);
        if (r == ARCHIVE_EOF) {
            return 0;
        }
        if (r != ARCHIVE_OK && r != ARCHIVE_WARN) {
            set_error(error_message, archive_error_string(archive->archive));
            return -1;
        }
        if ((archive_entry_mode(entry) & S_IFMT) == S_IFREG) {
            *name_out = archive_entry_pathname(entry);
            return 1;
        }
    }
}

ssize_t swipterm_archive_read(swipterm_archive *archive, void *buffer, size_t length, char **error_message) {
    if (error_message) {
        *error_message = NULL;
    }
    la_ssize_t n = archive_read_data(archive->archive, buffer, length);
    if (n < 0) {
        set_error(error_message, archive_error_string(archive->archive));
        return -1;
    }
    return (ssize_t)n;
}
