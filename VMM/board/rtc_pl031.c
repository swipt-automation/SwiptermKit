#include "board_internal.h"

#include <pthread.h>
#include <stdlib.h>
#include <time.h>

struct pl031 {
    pthread_mutex_t lock;
    pl031_state_t st;
};

pl031_t *pl031_create(void) {
    pl031_t *r = calloc(1, sizeof(*r));
    if (!r) {
        return NULL;
    }
    pthread_mutex_init(&r->lock, NULL);
    r->st.cr = 1U;
    return r;
}

void pl031_destroy(pl031_t *r) {
    if (!r) {
        return;
    }
    pthread_mutex_destroy(&r->lock);
    free(r);
}

uint64_t pl031_read(pl031_t *r, uint32_t offset) {
    uint32_t res = 0;

    pthread_mutex_lock(&r->lock);
    switch (offset) {
    case 0x000:
        res = (uint32_t)((int64_t)time(NULL) + r->st.tick_offset);
        break;
    case 0x004:
        res = r->st.mr;
        break;
    case 0x008:
        res = r->st.lr;
        break;
    case 0x00c:
        res = r->st.cr;
        break;
    case 0x010:
        res = r->st.imsc;
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
        break;
    }
    pthread_mutex_unlock(&r->lock);
    return res;
}

void pl031_write(pl031_t *r, uint32_t offset, uint64_t val) {
    uint32_t v = (uint32_t)val;

    pthread_mutex_lock(&r->lock);
    switch (offset) {
    case 0x004:
        r->st.mr = v;
        break;
    case 0x008:
        r->st.lr = v;
        r->st.tick_offset = (int64_t)v - (int64_t)time(NULL);
        break;
    case 0x00c:
        r->st.cr = v & 1U;
        break;
    case 0x010:
        r->st.imsc = v & 1U;
        break;
    default:
        break;
    }
    pthread_mutex_unlock(&r->lock);
}

void pl031_save_state(pl031_t *r, pl031_state_t *out) {
    pthread_mutex_lock(&r->lock);
    *out = r->st;
    pthread_mutex_unlock(&r->lock);
}

void pl031_load_state(pl031_t *r, const pl031_state_t *in) {
    pthread_mutex_lock(&r->lock);
    r->st = *in;
    pthread_mutex_unlock(&r->lock);
}
