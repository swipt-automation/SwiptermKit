#include "board_internal.h"
#include "machine.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct pl031 {
    pthread_mutex_t lock;
    vmm_machine_t *vm;
    int64_t tick_offset;
    uint32_t mr;
    uint32_t lr;
    uint32_t cr;
    uint32_t imsc;
    uint32_t ris;
} pl031_t;

void *pl031_create(vmm_machine_t *vm) {
    pl031_t *r = calloc(1, sizeof(*r));
    if (!r) {
        return NULL;
    }
    pthread_mutex_init(&r->lock, NULL);
    r->vm = vm;
    r->cr = 1U;
    return r;
}

void pl031_destroy(void *rtc) {
    if (!rtc) {
        return;
    }
    pl031_t *r = (pl031_t *)rtc;
    pthread_mutex_destroy(&r->lock);
    free(r);
}

uint64_t pl031_read(void *rtc, uint32_t offset, uint32_t size) {
    (void)size;
    if (!rtc) {
        return 0;
    }
    pl031_t *r = (pl031_t *)rtc;
    uint32_t res = 0;

    pthread_mutex_lock(&r->lock);
    switch (offset) {
    case 0x000:
        res = (uint32_t)((int64_t)time(NULL) + r->tick_offset);
        break;
    case 0x004:
        res = r->mr;
        break;
    case 0x008:
        res = r->lr;
        break;
    case 0x00c:
        res = r->cr;
        break;
    case 0x010:
        res = r->imsc;
        break;
    case 0x014:
        res = r->ris;
        break;
    case 0x018:
        res = r->ris & r->imsc;
        break;
    case 0xfe0:
        res = 0x31;
        break;
    case 0xfe4:
        res = 0x10;
        break;
    case 0xfe8:
        res = 0x04;
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
    pthread_mutex_unlock(&r->lock);
    return res;
}

void pl031_write(void *rtc, uint32_t offset, uint32_t size, uint64_t val) {
    (void)size;
    if (!rtc) {
        return;
    }
    pl031_t *r = (pl031_t *)rtc;
    uint32_t v = (uint32_t)val;
    bool update_irq = false;
    bool irq_level = false;

    pthread_mutex_lock(&r->lock);
    switch (offset) {
    case 0x004:
        r->mr = v;
        break;
    case 0x008:
        r->lr = v;
        r->tick_offset = (int64_t)v - (int64_t)time(NULL);
        break;
    case 0x00c:
        r->cr = v & 1U;
        break;
    case 0x010:
        r->imsc = v & 1U;
        update_irq = true;
        irq_level = (r->ris & r->imsc) != 0;
        break;
    case 0x01c:
        if (v & 1U) {
            r->ris = 0;
            update_irq = true;
            irq_level = false;
        }
        break;
    default:
        break;
    }
    pthread_mutex_unlock(&r->lock);

    if (update_irq) {
        gicv2_set_irq(r->vm, VMM_IRQ_RTC, irq_level);
    }
}

void pl031_save_state(const void *rtc, pl031_snapshot_t *out) {
    memset(out, 0, sizeof(*out));
    if (!rtc) {
        return;
    }
    pl031_t *r = (pl031_t *)rtc;
    pthread_mutex_lock(&r->lock);
    out->tick_offset = r->tick_offset;
    out->mr = r->mr;
    out->lr = r->lr;
    out->cr = r->cr;
    out->imsc = r->imsc;
    out->ris = r->ris;
    pthread_mutex_unlock(&r->lock);
}

void pl031_load_state(void *rtc, const pl031_snapshot_t *in) {
    if (!rtc || !in) {
        return;
    }
    pl031_t *r = (pl031_t *)rtc;
    pthread_mutex_lock(&r->lock);
    r->tick_offset = in->tick_offset;
    r->mr = in->mr;
    r->lr = in->lr;
    r->cr = in->cr;
    r->imsc = in->imsc;
    r->ris = in->ris;
    pthread_mutex_unlock(&r->lock);
}
