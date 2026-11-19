#include <string.h>

#include "internal.h"

#include "fs/devices.h"
#include "fs/dyndev.h"

#define DEVICE_MINOR_COUNT 256

static lock_t devices_lock = LOCK_INITIALIZER;
static void *device_contexts[DEVICE_MINOR_COUNT];
static int device_count;

static int device_open(int major, int minor, struct fd *fd) {
    fd->data = NULL;
    if (device_contexts[minor] == NULL)
        return _ENXIO;
    int err = 0;
    void *file = callbacks.device_open(device_contexts[minor], fd->flags, &err);
    if (file == NULL)
        return err < 0 ? err : _EIO;
    fd->data = file;
    return 0;
}

static ssize_t device_read(struct fd *fd, void *buffer, size_t length) {
    return callbacks.device_read(fd->data, buffer, length);
}

static ssize_t device_write(struct fd *fd, const void *buffer, size_t length) {
    return callbacks.device_write(fd->data, buffer, length);
}

static int device_close(struct fd *fd) {
    if (fd->data != NULL)
        callbacks.device_close(fd->data);
    return 0;
}

static struct dev_ops device_ops = {
    .open = device_open,
    .fd.read = device_read,
    .fd.write = device_write,
    .fd.close = device_close,
};

int swipterm_register_device(const char *path, void *context) {
    lock(&devices_lock);
    int minor = device_count < DEVICE_MINOR_COUNT ? device_count++ : -1;
    unlock(&devices_lock);
    if (minor < 0)
        return _ENOSPC;
    int err = dyn_dev_register(&device_ops, DEV_CHAR, DYN_DEV_MAJOR, minor);
    if (err < 0)
        return err;
    device_contexts[minor] = context;
    host_enter();
    err = generic_unlinkat(AT_PWD, path);
    if (err == 0 || err == _ENOENT)
        err = generic_mknodat(AT_PWD, path, S_IFCHR | 0666, dev_make(DYN_DEV_MAJOR, minor));
    host_leave();
    if (err < 0)
        device_contexts[minor] = NULL;
    return err;
}
