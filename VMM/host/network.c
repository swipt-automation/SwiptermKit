#include "../../Guest/swiptermd/wire_format.h"
#include "host_internal.h"
#include "network.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct swipterm_network {
    _Atomic int refcount;
    int wake_pipes[SWIPTERM_NETWORK_WAKE_COUNT][2];
    _Atomic bool armed[SWIPTERM_NETWORK_WAKE_COUNT];

    pthread_rwlock_t device_lock;
    virtio_dev_t *device;

    pthread_rwlock_t runtime_lock;
    virtio_dev_t *vsock;

    pthread_mutex_t state_lock;
    uint64_t session_nonce;
    uint64_t next_token;
    bool listeners_reset;
    swipterm_network_listener *listeners;
    size_t listener_count;
    size_t listener_capacity;
};

swipterm_network *swipterm_network_create(void) {
    swipterm_network *network = calloc(1, sizeof(*network));
    if (!network) {
        return NULL;
    }
    for (int wake = 0; wake < SWIPTERM_NETWORK_WAKE_COUNT; wake++) {
        network->wake_pipes[wake][0] = -1;
        network->wake_pipes[wake][1] = -1;
    }
    for (int wake = 0; wake < SWIPTERM_NETWORK_WAKE_COUNT; wake++) {
        if (pipe(network->wake_pipes[wake]) != 0) {
            for (int opened = 0; opened <= wake; opened++) {
                for (int end = 0; end < 2; end++) {
                    if (network->wake_pipes[opened][end] >= 0) {
                        close(network->wake_pipes[opened][end]);
                    }
                }
            }
            free(network);
            return NULL;
        }
        for (int end = 0; end < 2; end++) {
            (void)fcntl(network->wake_pipes[wake][end], F_SETFD, FD_CLOEXEC);
            (void)fcntl(network->wake_pipes[wake][end], F_SETFL, O_NONBLOCK);
        }
        atomic_init(&network->armed[wake], true);
    }
    atomic_init(&network->refcount, 1);
    pthread_rwlock_init(&network->device_lock, NULL);
    pthread_rwlock_init(&network->runtime_lock, NULL);
    pthread_mutex_init(&network->state_lock, NULL);
    return network;
}

void swipterm_network_retain(swipterm_network *network) {
    atomic_fetch_add_explicit(&network->refcount, 1, memory_order_relaxed);
}

void swipterm_network_release(swipterm_network *network) {
    if (atomic_fetch_sub_explicit(&network->refcount, 1, memory_order_acq_rel) != 1) {
        return;
    }
    for (int wake = 0; wake < SWIPTERM_NETWORK_WAKE_COUNT; wake++) {
        close(network->wake_pipes[wake][0]);
        close(network->wake_pipes[wake][1]);
    }
    pthread_rwlock_destroy(&network->device_lock);
    pthread_rwlock_destroy(&network->runtime_lock);
    pthread_mutex_destroy(&network->state_lock);
    free(network->listeners);
    free(network);
}

int swipterm_network_wake_fd(swipterm_network *network, swipterm_network_wake wake) {
    return network->wake_pipes[wake][0];
}

void swipterm_network_signal(swipterm_network *network, int wake) {
    if (!atomic_exchange_explicit(&network->armed[wake], false, memory_order_seq_cst)) {
        return;
    }
    uint8_t byte = 1;
    ssize_t written;
    do {
        written = write(network->wake_pipes[wake][1], &byte, 1);
    } while (written < 0 && errno == EINTR);
}

void swipterm_network_attach_device(swipterm_network *network, virtio_dev_t *net) {
    pthread_rwlock_wrlock(&network->device_lock);
    network->device = net;
    pthread_rwlock_unlock(&network->device_lock);
    swipterm_network_signal(network, SWIPTERM_NETWORK_WAKE_EVENT);
}

void swipterm_network_detach_device(swipterm_network *network, virtio_dev_t *net) {
    pthread_rwlock_wrlock(&network->device_lock);
    if (network->device == net) {
        network->device = NULL;
    }
    pthread_rwlock_unlock(&network->device_lock);
    swipterm_network_signal(network, SWIPTERM_NETWORK_WAKE_RX);
    swipterm_network_signal(network, SWIPTERM_NETWORK_WAKE_EVENT);
}

size_t swipterm_network_pop_tx(swipterm_network *network, uint8_t *arena, size_t capacity,
                               uint32_t *lengths, size_t max_frames) {
    atomic_store_explicit(&network->armed[SWIPTERM_NETWORK_WAKE_TX], true, memory_order_seq_cst);
    pthread_rwlock_rdlock(&network->device_lock);
    size_t count = network->device ? virtio_net_pop_tx(network->device, arena, capacity, lengths, max_frames) : 0;
    pthread_rwlock_unlock(&network->device_lock);
    return count;
}

int swipterm_network_push_rx(swipterm_network *network, const uint8_t *frame, size_t length, bool flush) {
    pthread_rwlock_rdlock(&network->device_lock);
    int rc = 0;
    if (network->device) {
        rc = virtio_net_push_rx(network->device, frame, length, flush);
        if (rc == -EAGAIN) {
            atomic_store_explicit(&network->armed[SWIPTERM_NETWORK_WAKE_RX], true, memory_order_seq_cst);
            rc = virtio_net_push_rx(network->device, frame, length, flush);
        }
    }
    pthread_rwlock_unlock(&network->device_lock);
    return rc;
}

uint64_t swipterm_network_features(swipterm_network *network) {
    pthread_rwlock_rdlock(&network->device_lock);
    uint64_t features = network->device ? virtio_net_features(network->device) : 0;
    pthread_rwlock_unlock(&network->device_lock);
    return features;
}

void swipterm_network_set_runtime(swipterm_network *network, virtio_dev_t *vsock) {
    pthread_rwlock_wrlock(&network->runtime_lock);
    network->vsock = vsock;
    pthread_mutex_lock(&network->state_lock);
    network->session_nonce = 0;
    network->listener_count = 0;
    network->listeners_reset = true;
    pthread_mutex_unlock(&network->state_lock);
    pthread_rwlock_unlock(&network->runtime_lock);
    swipterm_network_signal(network, SWIPTERM_NETWORK_WAKE_EVENT);
}

void swipterm_network_set_session(swipterm_network *network, uint64_t session_nonce) {
    pthread_mutex_lock(&network->state_lock);
    network->session_nonce = session_nonce;
    pthread_mutex_unlock(&network->state_lock);
}

void swipterm_network_push_listeners(swipterm_network *network, const swiptermd_mirror_entry_t *entries,
                                     size_t count) {
    pthread_mutex_lock(&network->state_lock);
    if (network->listener_count + count > network->listener_capacity) {
        size_t capacity = network->listener_capacity ? network->listener_capacity : 64;
        while (capacity < network->listener_count + count) {
            capacity *= 2;
        }
        swipterm_network_listener *listeners = realloc(network->listeners, capacity * sizeof(*listeners));
        if (!listeners) {
            pthread_mutex_unlock(&network->state_lock);
            return;
        }
        network->listeners = listeners;
        network->listener_capacity = capacity;
    }
    for (size_t i = 0; i < count; i++) {
        swipterm_network_listener *listener = &network->listeners[network->listener_count++];
        listener->family = entries[i].family;
        listener->action = entries[i].action;
        listener->port = entries[i].port;
        memcpy(listener->address, entries[i].addr, sizeof(listener->address));
    }
    pthread_mutex_unlock(&network->state_lock);
    swipterm_network_signal(network, SWIPTERM_NETWORK_WAKE_EVENT);
}

size_t swipterm_network_take_listeners(swipterm_network *network, swipterm_network_listener *listeners,
                                       size_t capacity, bool *reset_out) {
    atomic_store_explicit(&network->armed[SWIPTERM_NETWORK_WAKE_EVENT], true, memory_order_seq_cst);
    pthread_mutex_lock(&network->state_lock);
    *reset_out = network->listeners_reset;
    network->listeners_reset = false;
    size_t count = network->listener_count < capacity ? network->listener_count : capacity;
    memcpy(listeners, network->listeners, count * sizeof(*listeners));
    size_t remaining = network->listener_count - count;
    memmove(network->listeners, network->listeners + count, remaining * sizeof(*listeners));
    network->listener_count = remaining;
    pthread_mutex_unlock(&network->state_lock);
    if (remaining > 0) {
        swipterm_network_signal(network, SWIPTERM_NETWORK_WAKE_EVENT);
    }
    return count;
}

int swipterm_network_bridge(swipterm_network *network, bool inet6, int fd, uint16_t port) {
    pthread_rwlock_rdlock(&network->runtime_lock);
    if (!network->vsock) {
        pthread_rwlock_unlock(&network->runtime_lock);
        return -ENOTCONN;
    }
    pthread_mutex_lock(&network->state_lock);
    swiptermd_attach_hdr_t header = {
        .session_nonce = network->session_nonce,
        .role = SWIPTERMD_STREAM_TCP_MIRROR,
        .resource_id = swiptermd_mirror_pack_resource_id(inet6 ? SWIPTERMD_MIRROR_AF_INET6 : SWIPTERMD_MIRROR_AF_INET, port),
        .token = ++network->next_token,
    };
    pthread_mutex_unlock(&network->state_lock);
    uint8_t prefix[SWIPTERMD_ATTACH_HEADER_SIZE];
    swiptermd_attach_hdr_encode(prefix, &header);
    int rc = virtio_vsock_bridge_fd(network->vsock, SWIPTERMD_DATA_PORT, fd, prefix, sizeof(prefix)) < 0 ? -errno : 0;
    pthread_rwlock_unlock(&network->runtime_lock);
    return rc;
}
