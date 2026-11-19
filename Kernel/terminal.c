#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

#include "fs/tty.h"

#define OUTPUT_LIMIT (64 * 1024)
#define OUTPUT_ECHO_LIMIT (256 * 1024)
#define OUTPUT_INITIAL_CAPACITY (16 * 1024)

struct swipterm_terminal {
    atomic_int references;
    struct tty *tty;
    int number;
    int notify[2];

    lock_t lock;
    cond_t space;
    char *buffer;
    size_t head;
    size_t tail;
    size_t capacity;
    bool bound;
    bool closed;
    bool opened;
    int opens;
};

static void terminal_release(swipterm_terminal *terminal) {
    if (atomic_fetch_sub(&terminal->references, 1) != 1)
        return;
    close(terminal->notify[0]);
    close(terminal->notify[1]);
    cond_destroy(&terminal->space);
    free(terminal->buffer);
    free(terminal);
}

static void notify_reader(swipterm_terminal *terminal) {
    char byte = 0;
    (void) write(terminal->notify[1], &byte, 1);
}

static bool output_ended(swipterm_terminal *terminal) {
    return terminal->closed || (terminal->opened && terminal->opens == 0);
}

static bool output_append(swipterm_terminal *terminal, const char *data, size_t length) {
    size_t used = terminal->tail - terminal->head;
    if (terminal->tail + length > terminal->capacity) {
        memmove(terminal->buffer, terminal->buffer + terminal->head, used);
        terminal->head = 0;
        terminal->tail = used;
        if (used + length > terminal->capacity) {
            size_t capacity = terminal->capacity * 2;
            if (capacity < used + length)
                capacity = used + length;
            char *buffer = realloc(terminal->buffer, capacity);
            if (buffer == NULL)
                return false;
            terminal->buffer = buffer;
            terminal->capacity = capacity;
        }
    }
    memcpy(terminal->buffer + terminal->tail, data, length);
    terminal->tail += length;
    if (used == 0)
        notify_reader(terminal);
    return true;
}

static int terminal_driver_open(struct tty *tty) {
    swipterm_terminal *terminal = tty->data;
    if (terminal != NULL) {
        lock(&terminal->lock);
        terminal->opens++;
        terminal->opened = true;
        unlock(&terminal->lock);
    }
    return 0;
}

static int terminal_driver_close(struct tty *tty) {
    swipterm_terminal *terminal = tty->data;
    if (terminal != NULL) {
        lock(&terminal->lock);
        terminal->opens--;
        if (output_ended(terminal))
            notify_reader(terminal);
        unlock(&terminal->lock);
    }
    return 0;
}

static int terminal_driver_write(struct tty *tty, const void *data, size_t length, bool blocking) {
    swipterm_terminal *terminal = tty->data;
    if (terminal == NULL)
        return (int) length;
    const char *bytes = data;
    size_t written = 0;
    int err = 0;
    lock(&terminal->lock);
    while (written < length) {
        if (terminal->closed) {
            written = length;
            break;
        }
        size_t used = terminal->tail - terminal->head;
        size_t chunk;
        if (!blocking) {
            if (used >= OUTPUT_ECHO_LIMIT) {
                err = _EAGAIN;
                break;
            }
            chunk = length - written;
        } else if (used >= OUTPUT_LIMIT) {
            err = wait_for(&terminal->space, &terminal->lock, NULL);
            if (err < 0)
                break;
            continue;
        } else {
            chunk = length - written;
            if (chunk > OUTPUT_LIMIT - used)
                chunk = OUTPUT_LIMIT - used;
        }
        if (!output_append(terminal, bytes + written, chunk)) {
            err = _ENOMEM;
            break;
        }
        written += chunk;
    }
    unlock(&terminal->lock);
    return written > 0 ? (int) written : err;
}

static void terminal_driver_cleanup(struct tty *tty) {
    swipterm_terminal *terminal = tty->data;
    tty->data = NULL;
    if (terminal != NULL)
        terminal_release(terminal);
}

static const struct tty_driver_ops terminal_ops = {
    .open = terminal_driver_open,
    .close = terminal_driver_close,
    .write = terminal_driver_write,
    .cleanup = terminal_driver_cleanup,
};
static struct tty_driver terminal_driver = {.ops = &terminal_ops};

swipterm_terminal *swipterm_terminal_create(unsigned short columns, unsigned short rows, int *error_out) {
    swipterm_terminal *terminal = calloc(1, sizeof(*terminal));
    if (terminal == NULL) {
        *error_out = _ENOMEM;
        return NULL;
    }
    terminal->buffer = malloc(OUTPUT_INITIAL_CAPACITY);
    if (terminal->buffer == NULL || pipe(terminal->notify) < 0) {
        free(terminal->buffer);
        free(terminal);
        *error_out = _ENOMEM;
        return NULL;
    }
    for (int i = 0; i < 2; i++) {
        fcntl(terminal->notify[i], F_SETFL, O_NONBLOCK);
        fcntl(terminal->notify[i], F_SETFD, FD_CLOEXEC);
    }
    terminal->capacity = OUTPUT_INITIAL_CAPACITY;
    atomic_init(&terminal->references, 2);
    lock_init(&terminal->lock);
    cond_init(&terminal->space);

    host_enter();
    struct tty *tty = pty_open_fake(&terminal_driver);
    host_leave();
    if (IS_ERR(tty)) {
        *error_out = (int) PTR_ERR(tty);
        atomic_init(&terminal->references, 1);
        terminal_release(terminal);
        return NULL;
    }
    lock(&tty->lock);
    tty->data = terminal;
    tty->winsize = (struct winsize_) {.col = columns, .row = rows};
    unlock(&tty->lock);
    terminal->tty = tty;
    terminal->number = tty->num;
    return terminal;
}

void swipterm_terminal_release(swipterm_terminal *terminal) {
    terminal_release(terminal);
}

int swipterm_terminal_notify_fd(swipterm_terminal *terminal) {
    return terminal->notify[0];
}

int terminal_bind(swipterm_terminal *terminal) {
    lock(&terminal->lock);
    int err = terminal->closed ? _EIO : terminal->bound ? _EBUSY : 0;
    if (err == 0)
        terminal->bound = true;
    unlock(&terminal->lock);
    return err;
}

void terminal_unbind(swipterm_terminal *terminal) {
    lock(&terminal->lock);
    terminal->bound = false;
    unlock(&terminal->lock);
}

struct fd *terminal_open_slave(swipterm_terminal *terminal) {
    char path[32];
    snprintf(path, sizeof(path), "/dev/pts/%d", terminal->number);
    return generic_open(path, O_RDWR_, 0);
}

ssize_t swipterm_terminal_read(swipterm_terminal *terminal, void *buffer, size_t length) {
    lock(&terminal->lock);
    size_t used = terminal->tail - terminal->head;
    if (used == 0) {
        bool ended = output_ended(terminal);
        unlock(&terminal->lock);
        return ended ? 0 : _EAGAIN;
    }
    if (length > used)
        length = used;
    memcpy(buffer, terminal->buffer + terminal->head, length);
    terminal->head += length;
    if (terminal->head == terminal->tail) {
        terminal->head = terminal->tail = 0;
        if (terminal->capacity > OUTPUT_ECHO_LIMIT) {
            char *shrunk = realloc(terminal->buffer, OUTPUT_INITIAL_CAPACITY);
            if (shrunk != NULL) {
                terminal->buffer = shrunk;
                terminal->capacity = OUTPUT_INITIAL_CAPACITY;
            }
        }
    }
    notify(&terminal->space);
    unlock(&terminal->lock);
    return (ssize_t) length;
}

static struct tty *terminal_tty(swipterm_terminal *terminal) {
    lock(&ttys_lock);
    struct tty *tty = terminal->tty;
    if (tty != NULL) {
        lock(&tty->lock);
        tty->refcount++;
        unlock(&tty->lock);
    }
    unlock(&ttys_lock);
    return tty;
}

static void tty_put(struct tty *tty) {
    lock(&ttys_lock);
    tty_release(tty);
    unlock(&ttys_lock);
}

static bool input_ended(swipterm_terminal *terminal) {
    lock(&terminal->lock);
    bool ended = output_ended(terminal);
    unlock(&terminal->lock);
    return ended;
}

int swipterm_terminal_write(swipterm_terminal *terminal, const void *data, size_t length) {
    struct tty *tty = terminal_tty(terminal);
    if (tty == NULL)
        return _EIO;
    const char *bytes = data;
    int err = 0;
    while (length > 0 && !input_ended(terminal)) {
        ssize_t count = tty_input(tty, bytes, length, false);
        if (count > 0) {
            bytes += count;
            length -= count;
            continue;
        }
        if (count != _EAGAIN) {
            err = (int) count;
            break;
        }
        lock(&tty->lock);
        bool full = !tty->hung_up && tty->bufsize >= sizeof(tty->buf);
        bool line_overflow = full && (tty->termios.lflags & ICANON_) && memchr(tty->buf_flag, true, tty->bufsize) == NULL;
        if (full && !line_overflow) {
            struct timespec timeout = {.tv_sec = 1};
            wait_for_ignore_signals(&tty->consumed, &tty->lock, &timeout);
        }
        bool hung_up = tty->hung_up;
        unlock(&tty->lock);
        if (line_overflow || hung_up)
            break;
    }
    tty_put(tty);
    return err;
}

void swipterm_terminal_resize(swipterm_terminal *terminal, unsigned short columns, unsigned short rows) {
    struct tty *tty = terminal_tty(terminal);
    if (tty == NULL)
        return;
    lock(&tty->lock);
    tty->winsize = (struct winsize_) {.col = columns, .row = rows};
    pid_t_ foreground = tty->fg_group;
    unlock(&tty->lock);
    if (foreground != 0)
        send_group_signal(foreground, SIGWINCH_, SIGINFO_NIL);
    tty_put(tty);
}

void swipterm_terminal_close(swipterm_terminal *terminal) {
    lock(&ttys_lock);
    struct tty *tty = terminal->tty;
    terminal->tty = NULL;
    unlock(&ttys_lock);
    if (tty == NULL)
        return;

    lock(&terminal->lock);
    terminal->closed = true;
    notify(&terminal->space);
    notify_reader(terminal);
    unlock(&terminal->lock);

    lock(&tty->lock);
    pid_t_ session = tty->session;
    pid_t_ foreground = tty->fg_group;
    tty_hangup(tty);
    unlock(&tty->lock);
    pid_t_ groups[] = {foreground, session != foreground ? session : 0};
    for (int i = 0; i < 2; i++) {
        if (groups[i] != 0) {
            send_group_signal(groups[i], SIGHUP_, SIGINFO_NIL);
            send_group_signal(groups[i], SIGCONT_, SIGINFO_NIL);
        }
    }
    tty_put(tty);
}
