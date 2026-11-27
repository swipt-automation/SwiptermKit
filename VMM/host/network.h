#ifndef VMM_HOST_NETWORK_H
#define VMM_HOST_NETWORK_H

#include "../include/CSwiptermKit/swipterm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SWIPTERM_NETWORK_F_CSUM (1ULL << 0)
#define SWIPTERM_NETWORK_F_GUEST_CSUM (1ULL << 1)
#define SWIPTERM_NETWORK_F_MTU (1ULL << 3)
#define SWIPTERM_NETWORK_F_MAC (1ULL << 5)
#define SWIPTERM_NETWORK_F_GUEST_TSO4 (1ULL << 7)
#define SWIPTERM_NETWORK_F_GUEST_TSO6 (1ULL << 8)
#define SWIPTERM_NETWORK_F_HOST_TSO4 (1ULL << 11)
#define SWIPTERM_NETWORK_F_HOST_TSO6 (1ULL << 12)
#define SWIPTERM_NETWORK_F_MRG_RXBUF (1ULL << 15)
#define SWIPTERM_NETWORK_F_STATUS (1ULL << 16)
#define SWIPTERM_NETWORK_F_GUEST_USO4 (1ULL << 54)
#define SWIPTERM_NETWORK_F_GUEST_USO6 (1ULL << 55)
#define SWIPTERM_NETWORK_F_HOST_USO (1ULL << 56)

enum {
    SWIPTERM_NETWORK_MTU = 16384,
    SWIPTERM_NETWORK_HEADER_SIZE = 12,
    SWIPTERM_NETWORK_MAX_FRAME = SWIPTERM_NETWORK_HEADER_SIZE + 14 + 65535,
};

typedef enum {
    SWIPTERM_NETWORK_WAKE_TX = 0,
    SWIPTERM_NETWORK_WAKE_RX = 1,
    SWIPTERM_NETWORK_WAKE_EVENT = 2,
    SWIPTERM_NETWORK_WAKE_COUNT = 3,
} swipterm_network_wake;

enum {
    SWIPTERM_NETWORK_LISTENER_ADDED = 1,
};

typedef struct {
    uint8_t family;
    uint8_t action;
    uint16_t port;
    uint8_t address[16];
} swipterm_network_listener;

typedef struct virtio_dev virtio_dev_t;

swipterm_network *swipterm_network_create(void);
void swipterm_network_retain(swipterm_network *network);
void swipterm_network_release(swipterm_network *network);
void swipterm_network_attach_device(swipterm_network *network, virtio_dev_t *net);
void swipterm_network_detach_device(swipterm_network *network, virtio_dev_t *net);
void swipterm_network_signal(swipterm_network *network, swipterm_network_wake wake);
int swipterm_network_wake_fd(swipterm_network *network, swipterm_network_wake wake);
size_t swipterm_network_pop_tx(swipterm_network *network, uint8_t *arena, size_t capacity,
                               uint32_t *lengths, size_t max_frames);
int swipterm_network_push_rx(swipterm_network *network, const uint8_t *frame, size_t length, bool flush);
uint64_t swipterm_network_features(swipterm_network *network);
size_t swipterm_network_take_listeners(swipterm_network *network, swipterm_network_listener *listeners,
                                       size_t capacity, bool *reset_out);
int swipterm_network_bridge(swipterm_network *network, bool inet6, int fd, uint16_t port);

#endif
