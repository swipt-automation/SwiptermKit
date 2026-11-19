#include <arpa/inet.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <netdb.h>
#include <os/proc.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kernel/calls.h"
#include "kernel/fs.h"
#include "kernel/init.h"
#include "kernel/mm.h"
#include "kernel/task.h"
#include "fs/devices.h"
#include "fs/fd.h"
#include "fs/path.h"
#include "fs/sock.h"
#include "fs/tty.h"
#include "tools/fakefs.h"

#include "swipterm.h"
#include "overlay.h"

extern const char *proc_ish_version;

static lock_t kernel_lock = LOCK_INITIALIZER;
static bool booted;
static struct list terminals;
static void *console_context;
static swipterm_output_handler console_output;

static void handle_exit(struct task *task, int status);

static char *pack_strings(const char *const *strings, size_t *count_out) {
    size_t count = 0;
    size_t size = 1;
    for (const char *const *string = strings; *string != NULL; string++) {
        size += strlen(*string) + 1;
        count++;
    }
    char *packed = malloc(size);
    char *cursor = packed;
    for (const char *const *string = strings; *string != NULL; string++) {
        size_t length = strlen(*string) + 1;
        memcpy(cursor, *string, length);
        cursor += length;
    }
    *cursor = '\0';
    if (count_out != NULL)
        *count_out = count;
    return packed;
}

static int execute(const char *const *argv, const char *const *envp) {
    size_t argc;
    char *packed_argv = pack_strings(argv, &argc);
    char *packed_envp = pack_strings(envp, NULL);
    int err = do_execve(argv[0], argc, packed_argv, packed_envp);
    free(packed_argv);
    free(packed_envp);
    return err;
}

static void progress_callback(void *cookie, double progress, const char *message, bool *cancel) {
    void **handler = cookie;
    ((swipterm_progress_handler) handler[1])(handler[0], progress);
}

bool swipterm_root_import(const char *archive_path, const char *root_path,
                          void *context, swipterm_progress_handler progress,
                          char **error_message) {
    void *handler[] = {context, progress};
    struct fakefsify_error error;
    if (fakefs_import(archive_path, root_path, &error, (struct progress) {handler, progress_callback}))
        return true;
    *error_message = error.message;
    return false;
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
    if (console_output != NULL)
        console_output(console_context, buf, len);
    return (int) len;
}

static const struct tty_driver_ops console_ops = {
    .write = console_write,
};
DEFINE_TTY_DRIVER(console_driver, &console_ops, TTY_CONSOLE_MAJOR, 64);

static int read_file(const char *path, void **data_out, size_t *length_out) {
    struct fd *fd = generic_open(path, O_RDONLY_, 0);
    if (IS_ERR(fd))
        return (int) PTR_ERR(fd);
    size_t capacity = 4096;
    size_t length = 0;
    char *data = malloc(capacity);
    for (;;) {
        if (length == capacity) {
            capacity *= 2;
            data = realloc(data, capacity);
        }
        ssize_t count = fd->ops->read(fd, data + length, capacity - length);
        if (count < 0) {
            free(data);
            fd_close(fd);
            return (int) count;
        }
        if (count == 0)
            break;
        length += count;
    }
    fd_close(fd);
    *data_out = data;
    *length_out = length;
    return 0;
}

static int write_file(const char *path, const void *data, size_t length) {
    char parent[MAX_PATH];
    strlcpy(parent, path, sizeof(parent));
    for (char *slash = strchr(parent + 1, '/'); slash != NULL; slash = strchr(slash + 1, '/')) {
        *slash = '\0';
        generic_mkdirat(AT_PWD, parent, 0755);
        *slash = '/';
    }
    struct fd *fd = generic_open(path, O_WRONLY_ | O_CREAT_ | O_TRUNC_, 0644);
    if (IS_ERR(fd))
        return (int) PTR_ERR(fd);
    ssize_t count = fd->ops->write(fd, data, length);
    fd_close(fd);
    return count < 0 ? (int) count : 0;
}

static int apply_overlay(void) {
    void *data;
    size_t length;
    if (read_file("/ish/overlay-version", &data, &length) == 0) {
        char installed[16] = {0};
        memcpy(installed, data, length < sizeof(installed) - 1 ? length : sizeof(installed) - 1);
        free(data);
        if (atoi(installed) >= swipterm_overlay_version)
            return 0;
    }
    for (size_t i = 0; i < swipterm_overlay_file_count; i++) {
        int err = write_file(swipterm_overlay_files[i].path, swipterm_overlay_files[i].data, swipterm_overlay_files[i].length);
        if (err < 0)
            return err;
    }
    char version[16];
    int version_length = snprintf(version, sizeof(version), "%d\n", swipterm_overlay_version);
    return write_file("/ish/overlay-version", version, version_length);
}

static int boot(const swipterm_options *options) {
    start_memory_governor();

    char data_path[MAX_PATH];
    snprintf(data_path, sizeof(data_path), "%s/data", options->root_path);
    int err = mount_root(&fakefs, data_path);
    if (err < 0)
        return err;
    err = become_first_process();
    if (err < 0)
        return err;

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
    generic_setattrat(AT_PWD, "/", (struct attr) {.type = attr_mode, .mode = 0755}, false);
    do_mount(&procfs, "proc", "/proc", "", 0);
    do_mount(&devptsfs, "devpts", "/dev/pts", "", 0);
    err = apply_overlay();
    if (err < 0)
        return err;

    if (options->version != NULL)
        proc_ish_version = strdup(options->version);
    if (options->socket_prefix != NULL)
        sock_tmp_prefix = strdup(options->socket_prefix);
    list_init(&terminals);
    exit_hook = handle_exit;
    console_context = options->console_context;
    console_output = options->console_output;

    tty_drivers[TTY_CONSOLE_MAJOR] = &console_driver;
    set_console_device(TTY_CONSOLE_MAJOR, 1);
    err = create_stdio("/dev/console", TTY_CONSOLE_MAJOR, 1);
    if (err < 0)
        return err;
    err = execute(options->init_argv, options->init_envp);
    if (err < 0)
        return err;
    return task_start(current);
}

int swipterm_boot(const swipterm_options *options) {
    lock(&kernel_lock);
    int err = _EBUSY;
    if (!booted) {
        err = boot(options);
        booted = err == 0;
    }
    current = NULL;
    unlock(&kernel_lock);
    return err;
}

int swipterm_read_file(const char *path, void **data_out, size_t *length_out) {
    lock(&kernel_lock);
    current = pid_get_task(1);
    int err = read_file(path, data_out, length_out);
    current = NULL;
    unlock(&kernel_lock);
    return err;
}

int swipterm_write_file(const char *path, const void *data, size_t length) {
    lock(&kernel_lock);
    current = pid_get_task(1);
    int err = write_file(path, data, length);
    current = NULL;
    unlock(&kernel_lock);
    return err;
}

int swipterm_update_resolv_conf(void) {
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
    return swipterm_write_file("/etc/resolv.conf", conf, length);
}

struct swipterm_terminal {
    struct tty *tty;
    swipterm_terminal_handlers handlers;
    pid_t_ pid;
    struct list link;
    bool registered;
    lock_t output_lock;
    bool output_closed;
    lock_t flow_lock;
    cond_t resumed;
    bool suspended;
    bool closed;
};

static int terminal_write(struct tty *tty, const void *buf, size_t len, bool blocking) {
    struct swipterm_terminal *terminal = tty->data;
    if (terminal == NULL)
        return (int) len;
    if (blocking) {
        lock(&terminal->flow_lock);
        while (terminal->suspended && !terminal->closed)
            wait_for_ignore_signals(&terminal->resumed, &terminal->flow_lock, NULL);
        unlock(&terminal->flow_lock);
    }
    lock(&terminal->output_lock);
    if (!terminal->output_closed)
        terminal->handlers.output(terminal->handlers.context, buf, len);
    unlock(&terminal->output_lock);
    return (int) len;
}

static void terminal_cleanup(struct tty *tty) {
    struct swipterm_terminal *terminal = tty->data;
    tty->data = NULL;
    if (terminal == NULL)
        return;
    cond_destroy(&terminal->resumed);
    free(terminal);
}

static void handle_exit(struct task *task, int status) {
    struct swipterm_terminal *terminal, *next;
    list_for_each_entry_safe(&terminals, terminal, next, link) {
        if (terminal->pid != task->tgid)
            continue;
        list_remove(&terminal->link);
        terminal->registered = false;
        terminal->handlers.exit(terminal->handlers.context, status);
    }
}

static const struct tty_driver_ops terminal_ops = {
    .write = terminal_write,
    .cleanup = terminal_cleanup,
};
static struct tty_driver terminal_driver = {.ops = &terminal_ops};

static swipterm_terminal *terminal_open(const char *const *argv, const char *const *envp,
                                        struct winsize_ winsize, swipterm_terminal_handlers handlers,
                                        int *error_out) {
    int err = become_new_init_child();
    if (err < 0) {
        *error_out = err;
        return NULL;
    }
    struct tty *tty = pty_open_fake(&terminal_driver);
    if (IS_ERR(tty)) {
        *error_out = (int) PTR_ERR(tty);
        return NULL;
    }
    struct swipterm_terminal *terminal = calloc(1, sizeof(*terminal));
    terminal->tty = tty;
    lock_init(&terminal->output_lock);
    lock_init(&terminal->flow_lock);
    cond_init(&terminal->resumed);
    terminal->handlers = handlers;
    terminal->pid = current->pid;
    lock(&tty->lock);
    tty->data = terminal;
    tty->winsize = winsize;
    unlock(&tty->lock);

    char stdio_path[32];
    snprintf(stdio_path, sizeof(stdio_path), "/dev/pts/%d", tty->num);
    err = create_stdio(stdio_path, TTY_PSEUDO_SLAVE_MAJOR, tty->num);
    if (err >= 0)
        err = execute(argv, envp);
    if (err >= 0) {
        lock(&pids_lock);
        list_add(&terminals, &terminal->link);
        terminal->registered = true;
        unlock(&pids_lock);
        err = task_start(current);
    }
    if (err < 0) {
        *error_out = err;
        swipterm_terminal_close(terminal);
        return NULL;
    }
    return terminal;
}

swipterm_terminal *swipterm_terminal_open(const char *const *argv, const char *const *envp,
                                          unsigned short columns, unsigned short rows,
                                          swipterm_terminal_handlers handlers, int *error_out) {
    lock(&kernel_lock);
    swipterm_terminal *terminal = terminal_open(argv, envp, (struct winsize_) {.col = columns, .row = rows},
                                                handlers, error_out);
    current = NULL;
    unlock(&kernel_lock);
    return terminal;
}

void swipterm_terminal_send_input(swipterm_terminal *terminal, const void *data, size_t length) {
    struct tty *tty = terminal->tty;
    const char *cursor = data;
    while (length > 0) {
        ssize_t count = tty_input(tty, cursor, length, false);
        if (count > 0) {
            cursor += count;
            length -= count;
            continue;
        }
        if (count != _EAGAIN)
            return;
        int err = 0;
        lock(&tty->lock);
        if (!tty->hung_up && tty->bufsize >= sizeof(tty->buf)) {
            struct timespec timeout = {.tv_sec = 1};
            err = wait_for_ignore_signals(&tty->consumed, &tty->lock, &timeout);
        }
        unlock(&tty->lock);
        if (err == _ETIMEDOUT)
            return;
    }
}

void swipterm_terminal_resize(swipterm_terminal *terminal, unsigned short columns, unsigned short rows) {
    struct tty *tty = terminal->tty;
    lock(&tty->lock);
    tty_set_winsize(tty, (struct winsize_) {.col = columns, .row = rows});
    unlock(&tty->lock);
}

void swipterm_terminal_set_output_suspended(swipterm_terminal *terminal, bool suspended) {
    lock(&terminal->flow_lock);
    terminal->suspended = suspended;
    if (!suspended)
        notify(&terminal->resumed);
    unlock(&terminal->flow_lock);
}

void swipterm_terminal_close(swipterm_terminal *terminal) {
    struct tty *tty = terminal->tty;
    lock(&pids_lock);
    if (terminal->registered) {
        list_remove(&terminal->link);
        terminal->registered = false;
    }
    unlock(&pids_lock);
    lock(&terminal->output_lock);
    terminal->output_closed = true;
    unlock(&terminal->output_lock);
    lock(&terminal->flow_lock);
    terminal->closed = true;
    notify(&terminal->resumed);
    unlock(&terminal->flow_lock);
    lock(&tty->lock);
    tty_hangup(tty);
    unlock(&tty->lock);
    lock(&ttys_lock);
    tty_release(tty);
    unlock(&ttys_lock);
}
