#include "board_internal.h"

#include <pthread.h>
#include <stdlib.h>

enum : uint32_t {
    PL011_FR_RXFE = 1U << 4,
    PL011_FR_TXFE = 1U << 7,
    PL011_INT_TX = 1U << 5,
};

struct pl011 {
    pthread_mutex_t lock;
    vmm_machine_t *vm;
    pl011_state_t st;
};

static bool pl011_irq_level_locked(const pl011_t *u) {
    return (u->st.ris & u->st.imsc) != 0;
}

pl011_t *pl011_create(vmm_machine_t *vm) {
    pl011_t *u = calloc(1, sizeof(*u));
    if (!u) {
        return NULL;
    }
    pthread_mutex_init(&u->lock, NULL);
    u->vm = vm;
    u->st.cr = 0x0300U;
    u->st.ifls = 0x0012U;
    u->st.ris = PL011_INT_TX;
    return u;
}

void pl011_destroy(pl011_t *u) {
    if (!u) {
        return;
    }
    pthread_mutex_destroy(&u->lock);
    free(u);
}

uint64_t pl011_read(pl011_t *u, uint32_t offset) {
    uint32_t res = 0;

    pthread_mutex_lock(&u->lock);
    switch (offset) {
    case 0x018:
        res = PL011_FR_TXFE | PL011_FR_RXFE;
        break;
    case 0x020:
        res = u->st.ilpr;
        break;
    case 0x024:
        res = u->st.ibrd;
        break;
    case 0x028:
        res = u->st.fbrd;
        break;
    case 0x02c:
        res = u->st.lcr_h;
        break;
    case 0x030:
        res = u->st.cr;
        break;
    case 0x034:
        res = u->st.ifls;
        break;
    case 0x038:
        res = u->st.imsc;
        break;
    case 0x03c:
        res = u->st.ris;
        break;
    case 0x040:
        res = u->st.ris & u->st.imsc;
        break;
    case 0x048:
        res = u->st.dmacr;
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
        break;
    }
    pthread_mutex_unlock(&u->lock);
    return res;
}

void pl011_write(pl011_t *u, uint32_t offset, uint64_t val) {
    uint32_t v = (uint32_t)val;
    bool update_irq = false;
    bool irq_level = false;

    pthread_mutex_lock(&u->lock);
    switch (offset) {
    case 0x000:
        u->st.ris |= PL011_INT_TX;
        update_irq = true;
        irq_level = pl011_irq_level_locked(u);
        break;
    case 0x020:
        u->st.ilpr = v;
        break;
    case 0x024:
        u->st.ibrd = v & 0xffffU;
        break;
    case 0x028:
        u->st.fbrd = v & 0x3fU;
        break;
    case 0x02c:
        u->st.lcr_h = v & 0xffU;
        break;
    case 0x030:
        u->st.cr = v & 0xffffU;
        break;
    case 0x034:
        u->st.ifls = v & 0x3fU;
        break;
    case 0x038:
        u->st.imsc = v & 0x7ffU;
        update_irq = true;
        irq_level = pl011_irq_level_locked(u);
        break;
    case 0x044:
        u->st.ris &= ~(v & 0x7ffU);
        update_irq = true;
        irq_level = pl011_irq_level_locked(u);
        break;
    case 0x048:
        u->st.dmacr = v & 0x7U;
        break;
    default:
        break;
    }
    pthread_mutex_unlock(&u->lock);

    if (update_irq) {
        gicv2_set_irq(u->vm, VMM_IRQ_UART0, irq_level);
    }
}

void pl011_save_state(pl011_t *u, pl011_state_t *out) {
    pthread_mutex_lock(&u->lock);
    *out = u->st;
    pthread_mutex_unlock(&u->lock);
}

void pl011_load_state(pl011_t *u, const pl011_state_t *in) {
    pthread_mutex_lock(&u->lock);
    u->st = *in;
    pthread_mutex_unlock(&u->lock);
}
