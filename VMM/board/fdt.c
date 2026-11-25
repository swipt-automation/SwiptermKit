#include "board_internal.h"
#include "machine.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>

enum : uint32_t {
    FDT_MAGIC = 0xd00dfeedU,
    FDT_VERSION = 17U,
    FDT_LAST_COMP_VERSION = 16U,
    FDT_BEGIN_NODE = 0x00000001U,
    FDT_END_NODE = 0x00000002U,
    FDT_PROP = 0x00000003U,
    FDT_END = 0x00000009U,
    FDT_PHANDLE_GIC = 1U,
    FDT_PHANDLE_CLK = 2U,
    FDT_PHANDLE_L2 = 3U,
    FDT_CACHE_LINE_SIZE = 64U,
};

typedef struct fdt_buf {
    uint8_t *data;
    size_t len;
    size_t cap;
    bool err;
} fdt_buf_t;

typedef struct fdt_writer {
    fdt_buf_t dt_struct;
    fdt_buf_t dt_strings;
} fdt_writer_t;

static void buf_reserve(fdt_buf_t *b, size_t extra) {
    if (b->err) {
        return;
    }
    if (b->len + extra <= b->cap) {
        return;
    }
    size_t new_cap = b->cap ? b->cap * 2 : 1024;
    while (new_cap < b->len + extra) {
        new_cap *= 2;
    }
    uint8_t *p = realloc(b->data, new_cap);
    if (!p) {
        b->err = true;
        return;
    }
    b->data = p;
    b->cap = new_cap;
}

static void buf_put_bytes(fdt_buf_t *b, const void *src, size_t n) {
    if (n == 0) {
        return;
    }
    buf_reserve(b, n);
    if (b->err) {
        return;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
}

static void buf_put_be32(fdt_buf_t *b, uint32_t v) {
    uint8_t tmp[4] = {
        (uint8_t)(v >> 24),
        (uint8_t)(v >> 16),
        (uint8_t)(v >> 8),
        (uint8_t)v,
    };
    buf_put_bytes(b, tmp, 4);
}

static void buf_align4(fdt_buf_t *b) {
    size_t pad = (4 - (b->len & 3U)) & 3U;
    if (pad) {
        static const uint8_t zeros[4] = {0, 0, 0, 0};
        buf_put_bytes(b, zeros, pad);
    }
}

static uint32_t fdt_intern_string(fdt_writer_t *w, const char *s) {
    size_t slen = strlen(s) + 1;
    const uint8_t *cur = w->dt_strings.data;
    size_t off = 0;
    while (off < w->dt_strings.len) {
        size_t clen = strlen((const char *)(cur + off)) + 1;
        if (clen == slen && memcmp(cur + off, s, slen) == 0) {
            return (uint32_t)off;
        }
        off += clen;
    }
    uint32_t pos = (uint32_t)w->dt_strings.len;
    buf_put_bytes(&w->dt_strings, s, slen);
    return pos;
}

static void fdt_begin_node(fdt_writer_t *w, const char *name) {
    buf_put_be32(&w->dt_struct, FDT_BEGIN_NODE);
    buf_put_bytes(&w->dt_struct, name, strlen(name) + 1);
    buf_align4(&w->dt_struct);
}

static void fdt_prop(fdt_writer_t *w, const char *name, const void *val, size_t len) {
    uint32_t nameoff = fdt_intern_string(w, name);
    buf_put_be32(&w->dt_struct, FDT_PROP);
    buf_put_be32(&w->dt_struct, (uint32_t)len);
    buf_put_be32(&w->dt_struct, nameoff);
    buf_put_bytes(&w->dt_struct, val, len);
    buf_align4(&w->dt_struct);
}

static void fdt_prop_u32(fdt_writer_t *w, const char *name, uint32_t v) {
    uint8_t tmp[4] = {
        (uint8_t)(v >> 24),
        (uint8_t)(v >> 16),
        (uint8_t)(v >> 8),
        (uint8_t)v,
    };
    fdt_prop(w, name, tmp, 4);
}

static void fdt_prop_u64(fdt_writer_t *w, const char *name, uint64_t v) {
    uint8_t tmp[8] = {
        (uint8_t)(v >> 56), (uint8_t)(v >> 48), (uint8_t)(v >> 40), (uint8_t)(v >> 32),
        (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8),  (uint8_t)v,
    };
    fdt_prop(w, name, tmp, 8);
}

static void fdt_prop_cells(fdt_writer_t *w, const char *name, const uint32_t *cells, size_t count) {
    uint8_t tmp[64];
    for (size_t i = 0; i < count; i++) {
        uint32_t v = cells[i];
        tmp[i * 4 + 0] = (uint8_t)(v >> 24);
        tmp[i * 4 + 1] = (uint8_t)(v >> 16);
        tmp[i * 4 + 2] = (uint8_t)(v >> 8);
        tmp[i * 4 + 3] = (uint8_t)v;
    }
    fdt_prop(w, name, tmp, count * 4);
}

static void fdt_prop_reg2(fdt_writer_t *w, uint64_t addr, uint64_t size) {
    uint32_t cells[4] = {
        (uint32_t)(addr >> 32),
        (uint32_t)addr,
        (uint32_t)(size >> 32),
        (uint32_t)size,
    };
    fdt_prop_cells(w, "reg", cells, 4);
}

static uint32_t host_sysctl_u32(const char *name) {
    uint64_t value = 0;
    size_t len = sizeof(value);
    if (sysctlbyname(name, &value, &len, NULL, 0) != 0) {
        return 0;
    }
    return (uint32_t)value;
}

int fdt_build(const vmm_machine_t *vm, const vmm_config_t *config,
              void *buf, size_t buf_size, size_t *out_size) {
    if (!vm || !buf) {
        return -EINVAL;
    }

    uint32_t cpu_count = vm->cpu_count;

    fdt_writer_t w = {0};

    char product[64];
    size_t product_len = sizeof(product);
    char model[96];
    if (sysctlbyname("hw.product", product, &product_len, NULL, 0) == 0) {
        snprintf(model, sizeof(model), "Swipterm (%s)", product);
    } else {
        snprintf(model, sizeof(model), "Swipterm");
    }
    uint32_t l1i_size = host_sysctl_u32("hw.perflevel0.l1icachesize");
    uint32_t l1d_size = host_sysctl_u32("hw.perflevel0.l1dcachesize");
    uint32_t l2_size = host_sysctl_u32("hw.perflevel0.l2cachesize");

    fdt_begin_node(&w, "");
    fdt_prop(&w, "compatible", "linux,dummy-virt", sizeof("linux,dummy-virt"));
    fdt_prop(&w, "model", model, strlen(model) + 1);
    fdt_prop_u32(&w, "#address-cells", 2);
    fdt_prop_u32(&w, "#size-cells", 2);
    fdt_prop_u32(&w, "interrupt-parent", FDT_PHANDLE_GIC);

    fdt_begin_node(&w, "chosen");
    const char *cmdline = (config && config->cmdline && config->cmdline[0] != '\0')
                              ? config->cmdline
                              : "console=ttyAMA0 earlycon=pl011,0x09000000";
    fdt_prop(&w, "bootargs", cmdline, strlen(cmdline) + 1);
    fdt_prop(&w, "stdout-path", "/pl011@9000000", sizeof("/pl011@9000000"));
    if (config && config->initramfs && config->initramfs_size > 0) {
        uint64_t start = vmm_initrd_addr(vm->ram_size, config->initramfs_size);
        if (start != 0) {
            fdt_prop_u64(&w, "linux,initrd-start", start);
            fdt_prop_u64(&w, "linux,initrd-end", start + (uint64_t)config->initramfs_size);
        }
    }
    buf_put_be32(&w.dt_struct, FDT_END_NODE);

    fdt_begin_node(&w, "cpus");
    fdt_prop_u32(&w, "#address-cells", 1);
    fdt_prop_u32(&w, "#size-cells", 0);
    for (uint32_t i = 0; i < cpu_count; i++) {
        char name[16];
        snprintf(name, sizeof(name), "cpu@%x", i);
        fdt_begin_node(&w, name);
        fdt_prop(&w, "device_type", "cpu", sizeof("cpu"));
        fdt_prop(&w, "compatible", "arm,armv8", sizeof("arm,armv8"));
        fdt_prop_u32(&w, "reg", i);
        fdt_prop(&w, "enable-method", "psci", sizeof("psci"));
        if (l1i_size != 0) {
            fdt_prop_u32(&w, "i-cache-size", l1i_size);
            fdt_prop_u32(&w, "i-cache-line-size", FDT_CACHE_LINE_SIZE);
        }
        if (l1d_size != 0) {
            fdt_prop_u32(&w, "d-cache-size", l1d_size);
            fdt_prop_u32(&w, "d-cache-line-size", FDT_CACHE_LINE_SIZE);
        }
        if (l2_size != 0) {
            fdt_prop_u32(&w, "next-level-cache", FDT_PHANDLE_L2);
        }
        buf_put_be32(&w.dt_struct, FDT_END_NODE);
    }
    if (l2_size != 0) {
        fdt_begin_node(&w, "l2-cache");
        fdt_prop(&w, "compatible", "cache", sizeof("cache"));
        fdt_prop_u32(&w, "cache-level", 2);
        fdt_prop(&w, "cache-unified", NULL, 0);
        fdt_prop_u32(&w, "cache-size", l2_size);
        fdt_prop_u32(&w, "cache-line-size", FDT_CACHE_LINE_SIZE);
        fdt_prop_u32(&w, "phandle", FDT_PHANDLE_L2);
        buf_put_be32(&w.dt_struct, FDT_END_NODE);
    }
    buf_put_be32(&w.dt_struct, FDT_END_NODE);

    fdt_begin_node(&w, "psci");
    static const char psci_compat[] = "arm,psci-1.0\0arm,psci-0.2";
    fdt_prop(&w, "compatible", psci_compat, sizeof(psci_compat));
    const char *conduit = config && config->el2 ? "smc" : "hvc";
    fdt_prop(&w, "method", conduit, strlen(conduit) + 1);
    buf_put_be32(&w.dt_struct, FDT_END_NODE);

    fdt_begin_node(&w, "apb-pclk");
    fdt_prop(&w, "compatible", "fixed-clock", sizeof("fixed-clock"));
    fdt_prop_u32(&w, "#clock-cells", 0);
    fdt_prop_u32(&w, "clock-frequency", (uint32_t)VMM_TIMER_FREQ);
    fdt_prop(&w, "clock-output-names", "clk24mhz", sizeof("clk24mhz"));
    fdt_prop_u32(&w, "phandle", FDT_PHANDLE_CLK);
    buf_put_be32(&w.dt_struct, FDT_END_NODE);

    fdt_begin_node(&w, "intc@8000000");
    fdt_prop(&w, "compatible", "arm,cortex-a15-gic", sizeof("arm,cortex-a15-gic"));
    fdt_prop_u32(&w, "#interrupt-cells", 3);
    fdt_prop_u32(&w, "#address-cells", 2);
    fdt_prop_u32(&w, "#size-cells", 2);
    fdt_prop(&w, "interrupt-controller", NULL, 0);
    uint32_t gic_reg[16] = {
        (uint32_t)(VMM_GICD_BASE >> 32), (uint32_t)VMM_GICD_BASE,
        (uint32_t)(VMM_GICD_SIZE >> 32), (uint32_t)VMM_GICD_SIZE,
        (uint32_t)(VMM_GICC_BASE >> 32), (uint32_t)VMM_GICC_BASE,
        (uint32_t)(VMM_GICC_SIZE >> 32), (uint32_t)VMM_GICC_SIZE,
        (uint32_t)(VMM_GICH_BASE >> 32), (uint32_t)VMM_GICH_BASE,
        (uint32_t)(VMM_GICH_SIZE >> 32), (uint32_t)VMM_GICH_SIZE,
        (uint32_t)(VMM_GICV_BASE >> 32), (uint32_t)VMM_GICV_BASE,
        (uint32_t)(VMM_GICV_SIZE >> 32), (uint32_t)VMM_GICV_SIZE,
    };
    bool el2 = config && config->el2;
    fdt_prop_cells(&w, "reg", gic_reg, el2 ? 16 : 8);
    if (el2) {
        uint32_t maint_irq[3] = {1, VMM_IRQ_GIC_MAINT - VMM_IRQ_PPI_BASE, (((1U << cpu_count) - 1U) << 8) | 4U};
        fdt_prop_cells(&w, "interrupts", maint_irq, 3);
    }
    fdt_prop_u32(&w, "phandle", FDT_PHANDLE_GIC);
    buf_put_be32(&w.dt_struct, FDT_END_NODE);

    fdt_begin_node(&w, "timer");
    fdt_prop(&w, "compatible", "arm,armv8-timer", sizeof("arm,armv8-timer"));
    fdt_prop(&w, "always-on", NULL, 0);
    uint32_t ppi_flags = (((1U << cpu_count) - 1U) << 8) | 8U;
    uint32_t timer_irqs[15] = {
        1, VMM_IRQ_TIMER_SEC_PHYS - VMM_IRQ_PPI_BASE, ppi_flags,
        1, VMM_IRQ_TIMER_PHYS - VMM_IRQ_PPI_BASE, ppi_flags,
        1, VMM_IRQ_TIMER_VIRT - VMM_IRQ_PPI_BASE, ppi_flags,
        1, VMM_IRQ_TIMER_HYP_PHYS - VMM_IRQ_PPI_BASE, ppi_flags,
        1, VMM_IRQ_TIMER_HYP_VIRT - VMM_IRQ_PPI_BASE, ppi_flags,
    };
    fdt_prop_cells(&w, "interrupts", timer_irqs, 15);
    buf_put_be32(&w.dt_struct, FDT_END_NODE);

    fdt_begin_node(&w, "pl011@9000000");
    static const char pl011_compat[] = "arm,pl011\0arm,primecell";
    fdt_prop(&w, "compatible", pl011_compat, sizeof(pl011_compat));
    fdt_prop_reg2(&w, VMM_UART0_BASE, VMM_UART0_SIZE);
    uint32_t uart_irq[3] = {0, VMM_IRQ_UART0_SPI, 4};
    fdt_prop_cells(&w, "interrupts", uart_irq, 3);
    uint32_t uart_clks[2] = {FDT_PHANDLE_CLK, FDT_PHANDLE_CLK};
    fdt_prop_cells(&w, "clocks", uart_clks, 2);
    static const char uart_clk_names[] = "uartclk\0apb_pclk";
    fdt_prop(&w, "clock-names", uart_clk_names, sizeof(uart_clk_names));
    buf_put_be32(&w.dt_struct, FDT_END_NODE);

    fdt_begin_node(&w, "pl031@9010000");
    static const char pl031_compat[] = "arm,pl031\0arm,primecell";
    fdt_prop(&w, "compatible", pl031_compat, sizeof(pl031_compat));
    fdt_prop_reg2(&w, VMM_RTC_BASE, VMM_RTC_SIZE);
    uint32_t rtc_irq[3] = {0, VMM_IRQ_RTC_SPI, 4};
    fdt_prop_cells(&w, "interrupts", rtc_irq, 3);
    fdt_prop_u32(&w, "clocks", FDT_PHANDLE_CLK);
    fdt_prop(&w, "clock-names", "apb_pclk", sizeof("apb_pclk"));
    buf_put_be32(&w.dt_struct, FDT_END_NODE);

    for (uint32_t slot = 0; slot < VMM_VIRTIO_MMIO_SLOTS; slot++) {
        uint64_t addr = VMM_VIRTIO_MMIO_BASE + (uint64_t)slot * VMM_VIRTIO_MMIO_STRIDE;
        char name[32];
        snprintf(name, sizeof(name), "virtio_mmio@%llx", (unsigned long long)addr);
        fdt_begin_node(&w, name);
        fdt_prop(&w, "compatible", "virtio,mmio", sizeof("virtio,mmio"));
        fdt_prop_reg2(&w, addr, VMM_VIRTIO_MMIO_STRIDE);
        uint32_t irq[3] = {0, VMM_IRQ_VIRTIO_SPI_BASE + slot, 4};
        fdt_prop_cells(&w, "interrupts", irq, 3);
        fdt_prop(&w, "dma-coherent", NULL, 0);
        buf_put_be32(&w.dt_struct, FDT_END_NODE);
    }

    fdt_begin_node(&w, "memory@40000000");
    fdt_prop(&w, "device_type", "memory", sizeof("memory"));
    fdt_prop_reg2(&w, VMM_RAM_BASE, (uint64_t)vm->ram_size);
    buf_put_be32(&w.dt_struct, FDT_END_NODE);

    buf_put_be32(&w.dt_struct, FDT_END_NODE);
    buf_put_be32(&w.dt_struct, FDT_END);

    if (w.dt_struct.err || w.dt_strings.err) {
        free(w.dt_struct.data);
        free(w.dt_strings.data);
        return -ENOMEM;
    }

    uint32_t header_size = 40U;
    uint32_t rsvmap_off = header_size;
    uint32_t rsvmap_size = 16U;
    uint32_t struct_off = rsvmap_off + rsvmap_size;
    uint32_t struct_size = (uint32_t)w.dt_struct.len;
    uint32_t strings_off = struct_off + struct_size;
    uint32_t strings_size = (uint32_t)w.dt_strings.len;
    uint32_t total_size = (strings_off + strings_size + 7U) & ~7U;

    if (out_size) {
        *out_size = total_size;
    }
    if (total_size > buf_size) {
        free(w.dt_struct.data);
        free(w.dt_strings.data);
        return -ENOSPC;
    }

    uint8_t *dst = (uint8_t *)buf;
    memset(dst, 0, total_size);

    uint32_t hdr[10] = {
        FDT_MAGIC,
        total_size,
        struct_off,
        strings_off,
        rsvmap_off,
        FDT_VERSION,
        FDT_LAST_COMP_VERSION,
        0,
        strings_size,
        struct_size,
    };
    for (size_t i = 0; i < 10; i++) {
        uint32_t v = hdr[i];
        dst[i * 4 + 0] = (uint8_t)(v >> 24);
        dst[i * 4 + 1] = (uint8_t)(v >> 16);
        dst[i * 4 + 2] = (uint8_t)(v >> 8);
        dst[i * 4 + 3] = (uint8_t)v;
    }
    memcpy(dst + struct_off, w.dt_struct.data, struct_size);
    memcpy(dst + strings_off, w.dt_strings.data, strings_size);

    free(w.dt_struct.data);
    free(w.dt_strings.data);
    return 0;
}
