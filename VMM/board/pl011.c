#include "board_internal.h"
#include "machine.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

enum : uint32_t {
    PL011_RX_CAP = 4096,
    PL011_FR_RXFE = 1U << 4,
    PL011_FR_RXFF = 1U << 6,
    PL011_FR_TXFE = 1U << 7,
    PL011_INT_RX = 1U << 4,
    PL011_INT_TX = 1U << 5,
    PL011_INT_RT = 1U << 6,
};

typedef struct pl011 {
    pthread_mutex_t lock;
    vmm_machine_t *vm;
    uint32_t ilpr;
    uint32_t ibrd;
    uint32_t fbrd;
    uint32_t lcr_h;
    uint32_t cr;
    uint32_t ifls;
    uint32_t imsc;
    uint32_t ris;
    uint32_t dmacr;
    uint8_t rx_buf[PL011_RX_CAP];
    uint32_t rx_head;
    uint32_t rx_tail;
    uint32_t rx_count;
} pl011_t;

static bool pl011_irq_level_locked(const pl011_t *u) {
    return (u->ris & u->imsc) != 0;
}

void *pl011_create(vmm_machine_t *vm) {
    pl011_t *u = calloc(1, sizeof(*u));
    if (!u) {
        return NULL;
    }
    pthread_mutex_init(&u->lock, NULL);
    u->vm = vm;
    u->cr = 0x0300U;
    u->ifls = 0x0012U;
    u->ris = PL011_INT_TX;
    return u;
}

void pl011_destroy(void *uart) {
    if (!uart) {
        return;
    }
    pl011_t *u = (pl011_t *)uart;
    pthread_mutex_destroy(&u->lock);
    free(u);
}

void pl011_push_rx(vmm_machine_t *vm, const uint8_t *data, size_t len) {
    if (!vm || !vm->uart || !data || len == 0) {
        return;
    }
    pl011_t *u = (pl011_t *)vm->uart;
    pthread_mutex_lock(&u->lock);
    for (size_t i = 0; i < len && u->rx_count < PL011_RX_CAP; i++) {
        u->rx_buf[u->rx_tail] = data[i];
        u->rx_tail = (u->rx_tail + 1U) % PL011_RX_CAP;
        u->rx_count++;
    }
    if (u->rx_count > 0) {
        u->ris |= PL011_INT_RX | PL011_INT_RT;
    }
    bool irq = pl011_irq_level_locked(u);
    pthread_mutex_unlock(&u->lock);
    gicv2_set_irq(vm, VMM_IRQ_UART0, irq);
}

uint64_t pl011_read(void *uart, uint32_t offset, uint32_t size) {
    (void)size;
    if (!uart) {
        return 0;
    }
    pl011_t *u = (pl011_t *)uart;
    uint32_t res = 0;
    bool update_irq = false;
    bool irq_level = false;

    pthread_mutex_lock(&u->lock);
    switch (offset) {
    case 0x000:
        if (u->rx_count > 0) {
            res = u->rx_buf[u->rx_head];
            u->rx_head = (u->rx_head + 1U) % PL011_RX_CAP;
            u->rx_count--;
            if (u->rx_count == 0) {
                u->ris &= ~(PL011_INT_RX | PL011_INT_RT);
            }
            update_irq = true;
            irq_level = pl011_irq_level_locked(u);
        }
        break;
    case 0x004:
        res = 0;
        break;
    case 0x018:
        res = PL011_FR_TXFE;
        if (u->rx_count == 0) {
            res |= PL011_FR_RXFE;
        } else if (u->rx_count == PL011_RX_CAP) {
            res |= PL011_FR_RXFF;
        }
        break;
    case 0x020:
        res = u->ilpr;
        break;
    case 0x024:
        res = u->ibrd;
        break;
    case 0x028:
        res = u->fbrd;
        break;
    case 0x02c:
        res = u->lcr_h;
        break;
    case 0x030:
        res = u->cr;
        break;
    case 0x034:
        res = u->ifls;
        break;
    case 0x038:
        res = u->imsc;
        break;
    case 0x03c:
        res = u->ris;
        break;
    case 0x040:
        res = u->ris & u->imsc;
        break;
    case 0x048:
        res = u->dmacr;
        break;
    case 0xfe0:
        res = 0x11;
        break;
    case 0xfe4:
        res = 0x10;
        break;
    case 0xfe8:
        res = 0x14;
        break;
    case 0xfec:
        res = 0x00;
        break;
    case 0xff0:
        res = 0x0d;
        break;
    case 0xff4:
        res = 0xf0;
        break;
    case 0xff8:
        res = 0x05;
        break;
    case 0xffc:
        res = 0xb1;
        break;
    default:
        res = 0;
        break;
    }
    pthread_mutex_unlock(&u->lock);

    if (update_irq) {
        gicv2_set_irq(u->vm, VMM_IRQ_UART0, irq_level);
    }
    return res;
}

void pl011_write(void *uart, uint32_t offset, uint32_t size, uint64_t val) {
    (void)size;
    if (!uart) {
        return;
    }
    pl011_t *u = (pl011_t *)uart;
    uint32_t v = (uint32_t)val;
    bool emit_tx = false;
    uint8_t tx_byte = 0;
    bool update_irq = false;
    bool irq_level = false;

    pthread_mutex_lock(&u->lock);
    switch (offset) {
    case 0x000:
        tx_byte = (uint8_t)(v & 0xffU);
        emit_tx = true;
        u->ris |= PL011_INT_TX;
        update_irq = true;
        irq_level = pl011_irq_level_locked(u);
        break;
    case 0x020:
        u->ilpr = v;
        break;
    case 0x024:
        u->ibrd = v & 0xffffU;
        break;
    case 0x028:
        u->fbrd = v & 0x3fU;
        break;
    case 0x02c:
        u->lcr_h = v & 0xffU;
        break;
    case 0x030:
        u->cr = v & 0xffffU;
        break;
    case 0x034:
        u->ifls = v & 0x3fU;
        break;
    case 0x038:
        u->imsc = v & 0x7ffU;
        update_irq = true;
        irq_level = pl011_irq_level_locked(u);
        break;
    case 0x044:
        u->ris &= ~(v & 0x7ffU);
        if (u->rx_count > 0) {
            u->ris |= PL011_INT_RX;
        }
        update_irq = true;
        irq_level = pl011_irq_level_locked(u);
        break;
    case 0x048:
        u->dmacr = v & 0x7U;
        break;
    default:
        break;
    }
    pthread_mutex_unlock(&u->lock);

    if (emit_tx && u->vm && u->vm->console_output) {
        u->vm->console_output(u->vm->callback_ctx, &tx_byte, 1);
    }
    if (update_irq) {
        gicv2_set_irq(u->vm, VMM_IRQ_UART0, irq_level);
    }
}

void pl011_save_state(const void *uart, pl011_snapshot_t *out) {
    memset(out, 0, sizeof(*out));
    if (!uart) {
        return;
    }
    pl011_t *u = (pl011_t *)uart;
    pthread_mutex_lock(&u->lock);
    out->ilpr = u->ilpr;
    out->ibrd = u->ibrd;
    out->fbrd = u->fbrd;
    out->lcr_h = u->lcr_h;
    out->cr = u->cr;
    out->ifls = u->ifls;
    out->imsc = u->imsc;
    out->ris = u->ris;
    out->dmacr = u->dmacr;
    pthread_mutex_unlock(&u->lock);
}

void pl011_load_state(void *uart, const pl011_snapshot_t *in) {
    if (!uart || !in) {
        return;
    }
    pl011_t *u = (pl011_t *)uart;
    pthread_mutex_lock(&u->lock);
    u->ilpr = in->ilpr;
    u->ibrd = in->ibrd;
    u->fbrd = in->fbrd;
    u->lcr_h = in->lcr_h;
    u->cr = in->cr;
    u->ifls = in->ifls;
    u->imsc = in->imsc;
    u->ris = in->ris;
    u->dmacr = in->dmacr;
    u->rx_head = 0;
    u->rx_tail = 0;
    u->rx_count = 0;
    pthread_mutex_unlock(&u->lock);
}
