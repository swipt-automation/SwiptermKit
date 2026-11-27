#define _DARWIN_C_SOURCE

#include "host_internal.h"
#include "host_util.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <zstd.h>

enum {
    ARM64_IMAGE_MAGIC = 0x644d5241,
    ARM64_IMAGE_HEADER_SIZE = 64,
    KERNEL_PROBE_INPUT = 1U << 20,
    DECOMPRESS_INITIAL_CAP = 1U << 20,
    ZBOOT_HEADER_SIZE = 64,
    PE_SECTION_HEADER_SIZE = 40,
};

typedef enum codec {
    CODEC_RAW,
    CODEC_GZIP,
    CODEC_ZSTD,
    CODEC_XZ,
} codec_t;

static codec_t detect_codec(const uint8_t *in, size_t len) {
    if (len >= 2 && in[0] == 0x1F && in[1] == 0x8B) {
        return CODEC_GZIP;
    }
    if (len >= 4 && get_le32(in) == ZSTD_MAGICNUMBER) {
        return CODEC_ZSTD;
    }
    if (len >= sizeof(XZ_MAGIC) && memcmp(in, XZ_MAGIC, sizeof(XZ_MAGIC)) == 0) {
        return CODEC_XZ;
    }
    return CODEC_RAW;
}

typedef struct out_buffer {
    uint8_t *data;
    size_t len;
    size_t cap;
    size_t limit;
    bool prefix;
} out_buffer_t;

static int out_reserve(out_buffer_t *out) {
    if (out->len < out->cap) {
        return 0;
    }
    if (out->cap >= out->limit) {
        return out->prefix ? 1 : -EFBIG;
    }
    size_t cap = out->cap ? out->cap * 2 : DECOMPRESS_INITIAL_CAP;
    if (cap > out->limit) {
        cap = out->limit;
    }
    uint8_t *data = realloc(out->data, cap);
    if (!data) {
        return -ENOMEM;
    }
    out->data = data;
    out->cap = cap;
    return 0;
}

static int inflate_gzip(const uint8_t *in, size_t len, out_buffer_t *out) {
    z_stream zs = {0};
    if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) {
        return -ENOMEM;
    }
    int rc = 0;
    size_t consumed = 0;
    for (;;) {
        int reserve = out_reserve(out);
        if (reserve != 0) {
            rc = reserve > 0 ? 0 : reserve;
            break;
        }
        size_t in_left = len - consumed;
        size_t out_left = out->cap - out->len;
        zs.next_in = (Bytef *)(uintptr_t)(in + consumed);
        zs.avail_in = (uInt)(in_left > UINT32_MAX ? UINT32_MAX : in_left);
        zs.next_out = out->data + out->len;
        zs.avail_out = (uInt)(out_left > UINT32_MAX ? UINT32_MAX : out_left);
        uInt avail_in = zs.avail_in;
        uInt avail_out = zs.avail_out;
        int zrc = inflate(&zs, Z_NO_FLUSH);
        consumed += avail_in - zs.avail_in;
        out->len += avail_out - zs.avail_out;
        if (zrc == Z_STREAM_END) {
            if (len - consumed >= 2 && in[consumed] == 0x1F && in[consumed + 1] == 0x8B) {
                inflateReset(&zs);
                continue;
            }
            break;
        }
        if (zrc == Z_BUF_ERROR && consumed == len) {
            rc = out->prefix ? 0 : -EINVAL;
            break;
        }
        if (zrc != Z_OK && zrc != Z_BUF_ERROR) {
            rc = zrc == Z_MEM_ERROR ? -ENOMEM : -EINVAL;
            break;
        }
    }
    inflateEnd(&zs);
    return rc;
}

static bool zstd_frame_follows(const uint8_t *in, size_t len) {
    if (len < 4) {
        return false;
    }
    uint32_t magic = get_le32(in);
    return magic == ZSTD_MAGICNUMBER || (magic & ZSTD_MAGIC_SKIPPABLE_MASK) == ZSTD_MAGIC_SKIPPABLE_START;
}

static int inflate_zstd(const uint8_t *in, size_t len, out_buffer_t *out) {
    ZSTD_DCtx *dctx = ZSTD_createDCtx();
    if (!dctx) {
        return -ENOMEM;
    }
    (void)ZSTD_DCtx_setParameter(dctx, ZSTD_d_windowLogMax, 27);
    ZSTD_inBuffer input = {.src = in, .size = len, .pos = 0};
    int rc = 0;
    for (;;) {
        int reserve = out_reserve(out);
        if (reserve != 0) {
            rc = reserve > 0 ? 0 : reserve;
            break;
        }
        ZSTD_outBuffer output = {.dst = out->data, .size = out->cap, .pos = out->len};
        size_t ret = ZSTD_decompressStream(dctx, &output, &input);
        out->len = output.pos;
        if (ZSTD_isError(ret)) {
            rc = ZSTD_getErrorCode(ret) == ZSTD_error_memory_allocation ? -ENOMEM : -EINVAL;
            break;
        }
        if (ret == 0) {
            if (!zstd_frame_follows(in + input.pos, len - input.pos)) {
                break;
            }
            continue;
        }
        if (input.pos == len && output.pos < output.size) {
            rc = out->prefix ? 0 : -EINVAL;
            break;
        }
    }
    ZSTD_freeDCtx(dctx);
    return rc;
}

static int inflate_xz(const uint8_t *in, size_t len, out_buffer_t *out) {
    lzma_stream strm = {0};
    if (lzma_auto_decoder(&strm, 256ULL << 20, 0) != LZMA_OK) {
        return -ENOMEM;
    }
    strm.next_in = in;
    strm.avail_in = len;
    int rc = 0;
    for (;;) {
        int reserve = out_reserve(out);
        if (reserve != 0) {
            rc = reserve > 0 ? 0 : reserve;
            break;
        }
        strm.next_out = out->data + out->len;
        strm.avail_out = out->cap - out->len;
        size_t before = strm.avail_out;
        lzma_ret ret = lzma_code(&strm, LZMA_FINISH);
        out->len += before - strm.avail_out;
        if (ret == LZMA_STREAM_END) {
            break;
        }
        if (ret == LZMA_BUF_ERROR && strm.avail_in == 0) {
            rc = out->prefix ? 0 : -EINVAL;
            break;
        }
        if (ret != LZMA_OK) {
            rc = -EINVAL;
            break;
        }
    }
    lzma_end(&strm);
    return rc;
}

static int copy_raw(const uint8_t *in, size_t len, out_buffer_t *out) {
    size_t n = len;
    if (n > out->limit) {
        if (!out->prefix) {
            return -EFBIG;
        }
        n = out->limit;
    }
    out->data = malloc(n > 0 ? n : 1);
    if (!out->data) {
        return -ENOMEM;
    }
    memcpy(out->data, in, n);
    out->len = n;
    out->cap = n;
    return 0;
}

static int decompress_with(codec_t codec, const uint8_t *in, size_t len, size_t limit, bool prefix,
                           uint8_t **out, size_t *out_len) {
    out_buffer_t buf = {.limit = limit, .prefix = prefix};
    int rc = 0;
    switch (codec) {
    case CODEC_GZIP:
        rc = inflate_gzip(in, len, &buf);
        break;
    case CODEC_ZSTD:
        rc = inflate_zstd(in, len, &buf);
        break;
    case CODEC_XZ:
        rc = inflate_xz(in, len, &buf);
        break;
    case CODEC_RAW:
        rc = copy_raw(in, len, &buf);
        break;
    }
    if (rc < 0) {
        free(buf.data);
        return rc;
    }
    *out = buf.data;
    *out_len = buf.len;
    return 0;
}

int swipterm_decompress(const uint8_t *in, size_t len, size_t limit, uint8_t **out, size_t *out_len) {
    return decompress_with(detect_codec(in, len), in, len, limit, false, out, out_len);
}

typedef struct zboot_payload {
    codec_t codec;
    size_t offset;
    size_t size;
} zboot_payload_t;

static bool is_zboot(const uint8_t *file, size_t len) {
    return len >= ZBOOT_HEADER_SIZE && file[0] == 'M' && file[1] == 'Z' && memcmp(file + 4, "zimg", 4) == 0;
}

static int zboot_parse(const uint8_t *file, zboot_payload_t *payload) {
    const char *type = (const char *)file + 24;
    size_t type_len = strnlen(type, 32);
    if (type_len >= 4 && memcmp(type, "gzip", 4) == 0) {
        payload->codec = CODEC_GZIP;
    } else if (type_len >= 4 && memcmp(type, "zstd", 4) == 0) {
        payload->codec = CODEC_ZSTD;
    } else if ((type_len >= 2 && memcmp(type, "xz", 2) == 0) || (type_len >= 4 && memcmp(type, "lzma", 4) == 0)) {
        payload->codec = CODEC_XZ;
    } else {
        return -ENOTSUP;
    }
    payload->offset = get_le32(file + 8);
    payload->size = get_le32(file + 12);
    if (payload->size == 0 || payload->offset < ZBOOT_HEADER_SIZE) {
        return -ENOEXEC;
    }
    return 0;
}

static int stub_linux_section(const uint8_t *file, size_t len, size_t *offset_out, size_t *size_out,
                              size_t *needed_out) {
    if (len < ARM64_IMAGE_HEADER_SIZE || file[0] != 'M' || file[1] != 'Z') {
        return -ENOEXEC;
    }
    size_t pe = get_le32(file + 0x3C);
    if (pe > len || len - pe < 24) {
        *needed_out = pe + 24;
        return -EAGAIN;
    }
    if (memcmp(file + pe, "PE\0\0", 4) != 0) {
        return -ENOEXEC;
    }
    size_t table = pe + 24 + get_le16(file + pe + 20);
    size_t count = get_le16(file + pe + 6);
    if (table + count * PE_SECTION_HEADER_SIZE > len) {
        *needed_out = table + count * PE_SECTION_HEADER_SIZE;
        return -EAGAIN;
    }
    for (size_t i = 0; i < count; i++) {
        const uint8_t *section = file + table + i * PE_SECTION_HEADER_SIZE;
        if (memcmp(section, ".linux\0\0", 8) != 0) {
            continue;
        }
        uint32_t virtual_size = get_le32(section + 8);
        uint32_t raw_size = get_le32(section + 16);
        *offset_out = get_le32(section + 20);
        *size_out = virtual_size != 0 && virtual_size < raw_size ? virtual_size : raw_size;
        return *size_out == 0 || *offset_out < ARM64_IMAGE_HEADER_SIZE ? -ENOEXEC : 0;
    }
    return -ENOEXEC;
}

static int check_kernel_header(const uint8_t *image, size_t len) {
    if (len < ARM64_IMAGE_HEADER_SIZE || get_le32(image + 0x38) != ARM64_IMAGE_MAGIC) {
        return -ENOEXEC;
    }
    uint64_t flags = get_le64(image + 0x18);
    uint64_t page_size = (flags >> 1) & 3U;
    if ((flags & 1U) != 0 || page_size == 3) {
        return -ENOTSUP;
    }
    return page_size == 2 ? KERNEL_PAGE_16K : KERNEL_PAGE_4K;
}

int swipterm_kernel_probe(const uint8_t *file, size_t len, size_t *needed_out) {
    *needed_out = 0;
    size_t stub_offset = 0;
    size_t stub_size = 0;
    int stub = stub_linux_section(file, len, &stub_offset, &stub_size, needed_out);
    if (stub == -EAGAIN) {
        return stub;
    }
    if (stub == 0) {
        size_t want = stub_size < KERNEL_PROBE_INPUT ? stub_size : KERNEL_PROBE_INPUT;
        if (stub_offset > len || len - stub_offset < want) {
            *needed_out = stub_offset + want;
            return -EAGAIN;
        }
        size_t avail = len - stub_offset < stub_size ? len - stub_offset : stub_size;
        stub = swipterm_kernel_probe(file + stub_offset, avail, needed_out);
        if (stub == -EAGAIN) {
            *needed_out += stub_offset;
        }
        return stub;
    }
    codec_t codec = detect_codec(file, len);
    const uint8_t *payload = file;
    size_t payload_len = len;
    if (is_zboot(file, len)) {
        zboot_payload_t z;
        int rc = zboot_parse(file, &z);
        if (rc < 0) {
            return rc;
        }
        size_t want = z.size < KERNEL_PROBE_INPUT ? z.size : KERNEL_PROBE_INPUT;
        if (z.offset > len || len - z.offset < want) {
            *needed_out = z.offset + want;
            return -EAGAIN;
        }
        codec = z.codec;
        payload = file + z.offset;
        payload_len = want;
    } else if (codec == CODEC_RAW) {
        return check_kernel_header(file, len);
    }
    uint8_t *header = NULL;
    size_t header_len = 0;
    int rc = decompress_with(codec, payload, payload_len, ARM64_IMAGE_HEADER_SIZE, true, &header, &header_len);
    if (rc < 0) {
        return rc == -ENOMEM ? rc : -ENOEXEC;
    }
    rc = check_kernel_header(header, header_len);
    free(header);
    return rc;
}

int swipterm_kernel_image(const uint8_t *file, size_t len, uint8_t **image_out, size_t *image_len) {
    size_t stub_offset = 0;
    size_t stub_size = 0;
    size_t needed = 0;
    int stub = stub_linux_section(file, len, &stub_offset, &stub_size, &needed);
    if (stub == -EAGAIN || (stub == 0 && (stub_offset > len || len - stub_offset < stub_size))) {
        return -ENOEXEC;
    }
    if (stub == 0) {
        return swipterm_kernel_image(file + stub_offset, stub_size, image_out, image_len);
    }
    uint8_t *image = NULL;
    size_t size = 0;
    int rc;
    if (is_zboot(file, len)) {
        zboot_payload_t z;
        rc = zboot_parse(file, &z);
        if (rc == 0 && (z.offset > len || len - z.offset < z.size)) {
            rc = -ENOEXEC;
        }
        if (rc == 0) {
            rc = decompress_with(z.codec, file + z.offset, z.size, KERNEL_IMAGE_LIMIT, false, &image, &size);
        }
    } else {
        rc = swipterm_decompress(file, len, KERNEL_IMAGE_LIMIT, &image, &size);
    }
    if (rc < 0) {
        return rc;
    }
    rc = check_kernel_header(image, size);
    if (rc < 0) {
        free(image);
        return rc;
    }
    *image_out = image;
    *image_len = size;
    return 0;
}
