#include <limits.h>
#include <mach/mach.h>
#include <netdb.h>
#include <os/proc.h>
#include <poll.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

#include "kernel/init.h"
#include "kernel/mm.h"
#include "fs/devices.h"
#include "fs/real.h"
#include "fs/sock.h"
#include "fs/sockrestart.h"
#include "fs/tty.h"
#define ISH_INTERNAL
#include "fs/fake.h"

#include "overlay.h"

extern const char *proc_ish_version;
extern const char *uname_hostname_override;
extern void (*die_handler)(const char *msg);

swipterm_callbacks callbacks;
dispatch_queue_t events_queue;
struct task *init_task;
char *root_path;

static lock_t boot_lock = LOCK_INITIALIZER;
static bool booted;
static lock_t host_lock = LOCK_INITIALIZER;
static struct task *host_task;
static struct task *host_saved_current;

void host_enter(void) {
    lock(&host_lock);
    host_saved_current = current;
    current = host_task;
}

void host_leave(void) {
    lock(&host_task->sighand->lock);
    host_task->pending = 0;
    struct sigqueue *sigqueue, *next;
    list_for_each_entry_safe(&host_task->queue, sigqueue, next, queue) {
        list_remove(&sigqueue->queue);
        free(sigqueue);
    }
    unlock(&host_task->sighand->lock);
    current = host_saved_current;
    unlock(&host_lock);
}

bool signal_pending(void) {
    if (current == NULL || current->sighand == NULL)
        return false;
    lock(&current->sighand->lock);
    bool pending = (current->pending & ~current->blocked) != 0;
    unlock(&current->sighand->lock);
    return pending;
}

int wait_real_fd(int real_fd, short events) {
    for (;;) {
        if (signal_pending())
            return _EINTR;
        struct pollfd poll_fd = {.fd = real_fd, .events = events};
        int count = poll(&poll_fd, 1, 1000);
        if (count > 0)
            return 0;
        if (count < 0 && errno != EINTR)
            return errno_map();
    }
}

static int create_host_task(void) {
    struct task *task = task_create_(NULL);
    if (task == NULL)
        return _ENOMEM;
    lock(&pids_lock);
    pid_get(task->pid)->task = NULL;
    unlock(&pids_lock);

    struct tgroup *group = calloc(1, sizeof(*group));
    if (group == NULL)
        return _ENOMEM;
    list_init(&group->threads);
    list_init(&group->session);
    list_init(&group->pgroup);
    lock_init(&group->lock);
    cond_init(&group->child_exit);
    cond_init(&group->stopped_cond);
    group->leader = task;
    list_add(&group->threads, &task->group_links);
    task->group = group;
    task->tgid = task->pid;
    task->sighand = sighand_new();
    task->files = fdtable_new(3);
    task->fs = fs_info_new();
    if (task->sighand == NULL || IS_ERR(task->files) || task->fs == NULL)
        return _ENOMEM;

    struct task *saved_current = current;
    current = task;
    struct fd *root = generic_open("/", O_RDONLY_, 0);
    current = saved_current;
    if (IS_ERR(root))
        return (int) PTR_ERR(root);
    task->fs->root = root;
    task->fs->pwd = fd_retain(root);
    host_task = task;
    return 0;
}

static void update_memory_status(bool pressure_critical) {
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t) &info, &count) != KERN_SUCCESS)
        return;
    uint64_t available = os_proc_available_memory();
    if (info.phys_footprint == 0 || available == 0)
        return;
    ish_set_memory_status(info.phys_footprint + available, available, pressure_critical);
}

static void start_memory_governor(void) {
    size_t available = os_proc_available_memory();
    if (available != 0)
        ish_set_anon_page_limit((long) (available * 0.8 / getpagesize()));

    dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);
    static dispatch_source_t timer;
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, 250 * NSEC_PER_MSEC, 50 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(timer, ^{
        update_memory_status(false);
    });
    dispatch_resume(timer);
    static dispatch_source_t pressure;
    pressure = dispatch_source_create(DISPATCH_SOURCE_TYPE_MEMORYPRESSURE, 0,
                                      DISPATCH_MEMORYPRESSURE_WARN | DISPATCH_MEMORYPRESSURE_CRITICAL, queue);
    dispatch_source_set_event_handler(pressure, ^{
        update_memory_status((dispatch_source_get_data(pressure) & DISPATCH_MEMORYPRESSURE_CRITICAL) != 0);
    });
    dispatch_resume(pressure);
    update_memory_status(false);
}

static int console_write(struct tty *tty, const void *buf, size_t len, bool blocking) {
    if (callbacks.console_output != NULL)
        callbacks.console_output(buf, len);
    return (int) len;
}

static const struct tty_driver_ops console_ops = {
    .write = console_write,
};
DEFINE_TTY_DRIVER(console_driver, &console_ops, TTY_CONSOLE_MAJOR, 64);

static void handle_die(const char *message) {
    callbacks.panic(message);
}

static void deliver_halt(void *context) {
    if (callbacks.halt != NULL)
        callbacks.halt();
}

static void handle_halt(void) {
    dispatch_async_f(events_queue, NULL, deliver_halt);
}

static int apply_overlay(void) {
    void *data;
    size_t length;
    if (filesystem_read_file("/ish/overlay-version", &data, &length) == 0) {
        char installed[16] = {0};
        memcpy(installed, data, length < sizeof(installed) - 1 ? length : sizeof(installed) - 1);
        free(data);
        if (atoi(installed) >= swipterm_overlay_version)
            return 0;
    }
    for (size_t i = 0; i < swipterm_overlay_file_count; i++) {
        const struct swipterm_overlay_file *file = &swipterm_overlay_files[i];
        int err = filesystem_write_file(file->path, file->data, file->length, 0644);
        if (err < 0)
            return err;
    }
    char version[16];
    int version_length = snprintf(version, sizeof(version), "%d\n", swipterm_overlay_version);
    return filesystem_write_file("/ish/overlay-version", version, version_length, 0644);
}

static void create_device_nodes(void) {
    char tty_path[] = "/dev/ttyN";
    for (int number = 1; number <= 7; number++) {
        tty_path[8] = (char) ('0' + number);
        generic_mknodat(AT_PWD, tty_path, S_IFCHR | 0666, dev_make(TTY_CONSOLE_MAJOR, number));
    }
    generic_mknodat(AT_PWD, "/dev/tty", S_IFCHR | 0666, dev_make(TTY_ALTERNATE_MAJOR, DEV_TTY_MINOR));
    generic_mknodat(AT_PWD, "/dev/console", S_IFCHR | 0666, dev_make(TTY_ALTERNATE_MAJOR, DEV_CONSOLE_MINOR));
    generic_mknodat(AT_PWD, "/dev/ptmx", S_IFCHR | 0666, dev_make(TTY_ALTERNATE_MAJOR, DEV_PTMX_MINOR));
    generic_mknodat(AT_PWD, "/dev/null", S_IFCHR | 0666, dev_make(MEM_MAJOR, DEV_NULL_MINOR));
    generic_mknodat(AT_PWD, "/dev/zero", S_IFCHR | 0666, dev_make(MEM_MAJOR, DEV_ZERO_MINOR));
    generic_mknodat(AT_PWD, "/dev/full", S_IFCHR | 0666, dev_make(MEM_MAJOR, DEV_FULL_MINOR));
    generic_mknodat(AT_PWD, "/dev/random", S_IFCHR | 0666, dev_make(MEM_MAJOR, DEV_RANDOM_MINOR));
    generic_mknodat(AT_PWD, "/dev/urandom", S_IFCHR | 0666, dev_make(MEM_MAJOR, DEV_URANDOM_MINOR));
    generic_mkdirat(AT_PWD, "/dev/pts", 0755);
}

static int boot_host(const swipterm_options *options) {
    create_device_nodes();
    generic_setattrat(AT_PWD, "/", (struct attr) {.type = attr_mode, .mode = 0755}, false);
    lock(&mounts_lock);
    do_mount(&procfs, "proc", "/proc", "", 0);
    do_mount(&devptsfs, "devpts", "/dev/pts", "", 0);
    unlock(&mounts_lock);

    int err = filesystem_boot();
    if (err < 0)
        return err;
    err = apply_overlay();
    if (err < 0)
        return err;
    if (options->host_commands_directory != NULL) {
        err = commands_boot(options->host_commands_directory);
        if (err < 0)
            return err;
    }
    return 0;
}

static int boot(const swipterm_options *options, bool *mounted) {
    char data_path[PATH_MAX];
    snprintf(data_path, sizeof(data_path), "%s/data", options->root_path);
    char real_data_path[PATH_MAX];
    if (realpath(data_path, real_data_path) == NULL)
        return errno_map();
    int err = mount_root(&fakefs, real_data_path);
    if (err < 0)
        return err;
    *mounted = true;
    fakefs_set_rootfs_data_path(real_data_path);
    root_path = strdup(options->root_path);

    callbacks = options->callbacks;
    events_queue = dispatch_queue_create("SwiptermKit.Events", DISPATCH_QUEUE_SERIAL);
    if (callbacks.panic != NULL)
        die_handler = handle_die;
    if (options->hostname != NULL)
        uname_hostname_override = strdup(options->hostname);
    if (options->version != NULL)
        proc_ish_version = strdup(options->version);
    if (options->socket_prefix != NULL)
        sock_tmp_prefix = strdup(options->socket_prefix);
    start_memory_governor();

    err = become_first_process();
    if (err < 0)
        return err;
    init_task = current;
    err = create_host_task();
    if (err < 0)
        return err;

    tty_drivers[TTY_CONSOLE_MAJOR] = &console_driver;
    set_console_device(TTY_CONSOLE_MAJOR, 1);
    host_enter();
    err = boot_host(options);
    host_leave();
    if (err < 0)
        return err;

    exit_hook = process_task_exited;
    exec_hook = commands_exec;
    send_signal_hook = commands_signal;
    processes_boot();
    return 0;
}

int swipterm_boot(const swipterm_options *options) {
    lock(&boot_lock);
    int err = _EBUSY;
    if (!booted) {
        bool mounted = false;
        err = boot(options, &mounted);
        booted = mounted;
    }
    current = NULL;
    unlock(&boot_lock);
    return err;
}

int swipterm_start_init(const char *path, size_t argc, const char *const *argv, const char *const *envp) {
    if (path == NULL) {
        process_start_reaper();
        return 0;
    }
    halt_hook = handle_halt;
    current = init_task;
    int err = create_stdio("/dev/console", TTY_CONSOLE_MAJOR, 1);
    if (err == 0)
        err = process_start_init(path, argc, argv, envp);
    current = NULL;
    return err;
}

void swipterm_suspend(void) {
    sockrestart_on_suspend();
}

void swipterm_resume(void) {
    sockrestart_on_resume();
}

int swipterm_sync_dns(void) {
    struct __res_state state;
    if (res_ninit(&state) != 0)
        return _EIO;
    char conf[4096];
    size_t length = 0;
    if (state.dnsrch[0] != NULL) {
        length += snprintf(conf + length, sizeof(conf) - length, "search");
        for (int i = 0; state.dnsrch[i] != NULL && length < sizeof(conf); i++)
            length += snprintf(conf + length, sizeof(conf) - length, " %s", state.dnsrch[i]);
        if (length < sizeof(conf))
            length += snprintf(conf + length, sizeof(conf) - length, "\n");
    }
    union res_sockaddr_union servers[MAXNS];
    int count = res_getservers(&state, servers, MAXNS);
    for (int i = 0; i < count && length < sizeof(conf); i++) {
        if (servers[i].sin.sin_len == 0)
            continue;
        char address[NI_MAXHOST];
        if (getnameinfo((struct sockaddr *) &servers[i].sin, servers[i].sin.sin_len,
                        address, sizeof(address), NULL, 0, NI_NUMERICHOST) != 0)
            continue;
        length += snprintf(conf + length, sizeof(conf) - length, "nameserver %s\n", address);
    }
    res_ndestroy(&state);
    if (length > sizeof(conf))
        length = sizeof(conf);
    host_enter();
    int err = filesystem_write_file("/etc/resolv.conf", conf, length, 0644);
    host_leave();
    return err;
}
