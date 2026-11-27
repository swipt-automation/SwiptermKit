#ifndef VMM_VIRTIO_VIRTIO_MMIO_H
#define VMM_VIRTIO_VIRTIO_MMIO_H

#include "../common/vmm_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct virtio_dev virtio_dev_t;
typedef struct swipterm_network swipterm_network;

uint64_t virtio_mmio_read(virtio_dev_t *dev, uint32_t offset, uint32_t size);
void virtio_mmio_write(virtio_dev_t *dev, uint32_t offset, uint32_t size, uint64_t val);
void virtio_dev_destroy(virtio_dev_t *dev);

virtio_dev_t *virtio_blk_create(vmm_machine_t *vm, uint32_t slot, int fd, bool read_only);
void virtio_blk_flush_host(virtio_dev_t *blk);
virtio_dev_t *virtio_vsock_create(vmm_machine_t *vm, uint32_t slot, uint64_t guest_cid);
virtio_dev_t *virtio_net_create(vmm_machine_t *vm, uint32_t slot, const uint8_t mac[6],
                                swipterm_network *network);
size_t virtio_net_pop_tx(virtio_dev_t *net, uint8_t *arena, size_t capacity, uint32_t *lengths, size_t max_frames);
int virtio_net_push_rx(virtio_dev_t *net, const uint8_t *frame, size_t length, bool flush);
uint64_t virtio_net_features(virtio_dev_t *net);
virtio_dev_t *virtio_balloon_create(vmm_machine_t *vm, uint32_t slot);
virtio_dev_t *virtio_rng_create(vmm_machine_t *vm, uint32_t slot);
virtio_dev_t *virtio_fs_create(vmm_machine_t *vm, uint32_t slot,
                               const char *tag, const char *host_root, bool read_only);
virtio_dev_t *virtio_gpu_create(vmm_machine_t *vm, uint32_t slot);

typedef void (*virtio_vsock_accept_cb)(void *ctx, int host_fd);

int virtio_vsock_accept_control(virtio_dev_t *vsock, uint32_t host_port, int timeout_ms);
int virtio_vsock_listen_host(virtio_dev_t *vsock, uint32_t host_port,
                             virtio_vsock_accept_cb cb, void *ctx);
void virtio_vsock_unlisten_host(virtio_dev_t *vsock, uint32_t host_port);
int virtio_vsock_bridge_fd(virtio_dev_t *vsock, uint32_t guest_port, int peer_fd,
                           const void *prefix, size_t prefix_length);
void virtio_vsock_notify_halt(virtio_dev_t *vsock);

#endif
