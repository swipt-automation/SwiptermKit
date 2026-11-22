#ifndef VMM_VIRTIO_VIRTIO_MMIO_H
#define VMM_VIRTIO_VIRTIO_MMIO_H

#include "../common/vmm_types.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum : uint32_t {
    VIRTIO_MMIO_MAGIC_VALUE = 0x000,
    VIRTIO_MMIO_VERSION = 0x004,
    VIRTIO_MMIO_DEVICE_ID = 0x008,
    VIRTIO_MMIO_VENDOR_ID = 0x00c,
    VIRTIO_MMIO_DEVICE_FEATURES = 0x010,
    VIRTIO_MMIO_DEVICE_FEATURES_SEL = 0x014,
    VIRTIO_MMIO_DRIVER_FEATURES = 0x020,
    VIRTIO_MMIO_DRIVER_FEATURES_SEL = 0x024,
    VIRTIO_MMIO_QUEUE_SEL = 0x030,
    VIRTIO_MMIO_QUEUE_NUM_MAX = 0x034,
    VIRTIO_MMIO_QUEUE_NUM = 0x038,
    VIRTIO_MMIO_QUEUE_READY = 0x044,
    VIRTIO_MMIO_QUEUE_NOTIFY = 0x050,
    VIRTIO_MMIO_INTERRUPT_STATUS = 0x060,
    VIRTIO_MMIO_INTERRUPT_ACK = 0x064,
    VIRTIO_MMIO_STATUS = 0x070,
    VIRTIO_MMIO_QUEUE_DESC_LOW = 0x080,
    VIRTIO_MMIO_QUEUE_DESC_HIGH = 0x084,
    VIRTIO_MMIO_QUEUE_DRIVER_LOW = 0x090,
    VIRTIO_MMIO_QUEUE_DRIVER_HIGH = 0x094,
    VIRTIO_MMIO_QUEUE_DEVICE_LOW = 0x0a0,
    VIRTIO_MMIO_QUEUE_DEVICE_HIGH = 0x0a4,
    VIRTIO_MMIO_SHM_SEL = 0x0ac,
    VIRTIO_MMIO_SHM_LEN_LOW = 0x0b0,
    VIRTIO_MMIO_SHM_LEN_HIGH = 0x0b4,
    VIRTIO_MMIO_SHM_BASE_LOW = 0x0b8,
    VIRTIO_MMIO_SHM_BASE_HIGH = 0x0bc,
    VIRTIO_MMIO_QUEUE_RESET = 0x0c0,
    VIRTIO_MMIO_CONFIG_GENERATION = 0x0fc,
    VIRTIO_MMIO_CONFIG = 0x100,
};

#define VIRTIO_MMIO_MAGIC 0x74726976U
#define VIRTIO_MMIO_VERSION_2 2U
#define VIRTIO_VENDOR_SWIPTERM 0x53574950U

enum : uint32_t {
    VIRTIO_ID_NET = 1,
    VIRTIO_ID_BLOCK = 2,
    VIRTIO_ID_RNG = 4,
    VIRTIO_ID_BALLOON = 5,
    VIRTIO_ID_VSOCK = 19,
    VIRTIO_ID_FS = 26,
};

enum : uint32_t {
    VIRTIO_STATUS_ACKNOWLEDGE = 1U << 0,
    VIRTIO_STATUS_DRIVER = 1U << 1,
    VIRTIO_STATUS_DRIVER_OK = 1U << 2,
    VIRTIO_STATUS_FEATURES_OK = 1U << 3,
    VIRTIO_STATUS_DEVICE_NEEDS_RESET = 1U << 6,
    VIRTIO_STATUS_FAILED = 1U << 7,
};

enum : uint32_t {
    VIRTIO_MMIO_INT_VRING = 1U << 0,
    VIRTIO_MMIO_INT_CONFIG = 1U << 1,
};

#define VIRTIO_F_INDIRECT_DESC (1ULL << 28)
#define VIRTIO_F_RING_EVENT_IDX (1ULL << 29)
#define VIRTIO_F_VERSION_1 (1ULL << 32)

enum : uint16_t {
    VRING_DESC_F_NEXT = 1,
    VRING_DESC_F_WRITE = 2,
    VRING_DESC_F_INDIRECT = 4,
};

typedef struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} vring_desc_t;

typedef struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} vring_avail_t;

typedef struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} vring_used_elem_t;

typedef struct vring_used {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem_t ring[];
} vring_used_t;

typedef struct virtio_iov {
    void *base;
    size_t len;
    bool write;
} virtio_iov_t;

typedef struct virtqueue {
    uint32_t index;
    uint32_t num_max;
    uint32_t num;
    _Atomic uint32_t ready;
    uint64_t desc_gpa;
    uint64_t avail_gpa;
    uint64_t used_gpa;
    uint16_t last_avail_idx;
    uint16_t used_idx;
} virtqueue_t;

typedef struct virtio_dev virtio_dev_t;
typedef struct swipterm_network swipterm_network;

typedef struct virtio_dev_ops {
    uint64_t (*get_features)(virtio_dev_t *dev);
    void (*set_features)(virtio_dev_t *dev, uint64_t features);
    uint64_t (*read_config)(virtio_dev_t *dev, uint32_t offset, uint32_t size);
    void (*write_config)(virtio_dev_t *dev, uint32_t offset, uint32_t size, uint64_t val);
    void (*queue_notify)(virtio_dev_t *dev, uint32_t queue_idx);
    void (*reset)(virtio_dev_t *dev);
    void (*destroy)(virtio_dev_t *dev);
} virtio_dev_ops_t;

struct virtio_dev {
    vmm_machine_t *vm;
    uint32_t slot;
    uint32_t device_id;
    uint32_t vendor_id;
    uint32_t irq_intid;
    _Atomic uint32_t status;
    uint32_t config_generation;
    uint32_t device_features_sel;
    uint32_t driver_features_sel;
    uint64_t device_features;
    uint64_t driver_features;
    uint32_t queue_sel;
    uint32_t num_queues;
    virtqueue_t *queues;
    pthread_mutex_t irq_lock;
    _Atomic uint32_t interrupt_status;
    const virtio_dev_ops_t *ops;
    void *priv;
};

uint64_t virtio_mmio_read(virtio_dev_t *dev, uint32_t offset, uint32_t size);
void virtio_mmio_write(virtio_dev_t *dev, uint32_t offset, uint32_t size, uint64_t val);
void virtio_raise_irq(virtio_dev_t *dev, uint32_t isr_bits);
int virtqueue_pop(virtio_dev_t *dev, virtqueue_t *vq, virtio_iov_t *iov, size_t max_iov,
                  uint16_t *head_out, size_t *out_count, size_t *in_count);
void virtqueue_push(virtio_dev_t *dev, virtqueue_t *vq, uint16_t head, uint32_t len);
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

typedef void (*virtio_vsock_accept_cb)(void *ctx, int host_fd, uint32_t guest_port, uint32_t host_port);

int virtio_vsock_connect_host(virtio_dev_t *vsock, uint32_t guest_port);
int virtio_vsock_accept_control(virtio_dev_t *vsock, uint32_t host_port, int timeout_ms);
int virtio_vsock_listen_host(virtio_dev_t *vsock, uint32_t host_port,
                             virtio_vsock_accept_cb cb, void *ctx);
void virtio_vsock_unlisten_host(virtio_dev_t *vsock, uint32_t host_port);
int virtio_vsock_bridge_fd(virtio_dev_t *vsock, uint32_t guest_port, int peer_fd,
                           const void *prefix, size_t prefix_length);
void virtio_vsock_notify_halt(virtio_dev_t *vsock);

void swipterm_network_retain(swipterm_network *network);
void swipterm_network_release(swipterm_network *network);
void swipterm_network_attach_device(swipterm_network *network, virtio_dev_t *net);
void swipterm_network_detach_device(swipterm_network *network, virtio_dev_t *net);
void swipterm_network_signal(swipterm_network *network, int wake);

#endif
