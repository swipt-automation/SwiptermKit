#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

#include "fs/path.h"
#include "fs/poll.h"
#include "fs/real.h"

#define CHANNEL_BUFFER_SIZE (64 * 1024)

struct command {
    char *name;
    void *context;
};

struct swipterm_invocation {
    atomic_int references;
    pid_t_ tgid;
    void *context;
    bool aborted;
    bool ended;
    int done_fd;
    struct list link;
};

struct channel {
    struct fd *source;
    struct fd *sink;
    bool host_source;
    char *buffer;
    size_t offset;
    size_t length;
    bool ended;
};

struct session {
    swipterm_invocation *invocation;
    struct channel channels[3];
    struct fd *done;
    int done_fds[2];
    int host_fds[3];
    size_t argc;
    char **argv;
    char **envp;
    bool finished;
    int status;
};

static char *commands_directory;
static lock_t commands_lock = LOCK_INITIALIZER;
static struct command *commands;
static size_t command_count;
static lock_t invocations_lock = LOCK_INITIALIZER;
static struct list invocations = LIST_INITIALIZER(invocations);
static const struct fd_ops host_pipe_fdops = {
    .poll = realfs_poll,
    .close = realfs_close,
};

int commands_boot(const char *directory) {
    char normalized[MAX_PATH];
    int err = filesystem_create_directory(directory, 0755, true);
    if (err == 0)
        err = path_normalize(AT_PWD, directory, normalized, N_SYMLINK_FOLLOW);
    if (err < 0)
        return err;
    if (normalized[0] == '\0')
        return _EINVAL;
    lock(&mounts_lock);
    err = do_mount(&tmpfs, "tmpfs", normalized, "", 0);
    unlock(&mounts_lock);
    if (err < 0)
        return err;
    generic_setattrat(AT_PWD, normalized, (struct attr) {.type = attr_mode, .mode = 0755}, true);
    commands_directory = strdup(normalized);
    return 0;
}

int swipterm_register_command(const char *name, void *context) {
    if (commands_directory == NULL)
        return _ENOENT;
    if (name[0] == '\0' || strchr(name, '/') != NULL || strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ||
        strlen(name) >= MAX_NAME)
        return _EINVAL;
    lock(&commands_lock);
    int err = 0;
    for (size_t i = 0; i < command_count; i++) {
        if (strcmp(commands[i].name, name) == 0)
            err = _EEXIST;
    }
    if (err == 0) {
        struct command *grown = realloc(commands, (command_count + 1) * sizeof(*commands));
        if (grown == NULL)
            err = _ENOMEM;
        else
            commands = grown;
    }
    if (err == 0) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", commands_directory, name);
        host_enter();
        struct fd *fd = generic_open(path, O_WRONLY_ | O_CREAT_ | O_TRUNC_, 0755);
        host_leave();
        if (IS_ERR(fd)) {
            err = (int) PTR_ERR(fd);
        } else {
            fd_close(fd);
            commands[command_count++] = (struct command) {.name = strdup(name), .context = context};
        }
    }
    unlock(&commands_lock);
    return err;
}

static void *find_command(const char *file, char *name) {
    if (commands_directory == NULL)
        return NULL;
    char path[MAX_PATH];
    if (path_normalize(AT_PWD, file, path, N_SYMLINK_FOLLOW) < 0)
        return NULL;
    size_t length = strlen(commands_directory);
    if (strncmp(path, commands_directory, length) != 0 || path[length] != '/')
        return NULL;
    void *context = NULL;
    lock(&commands_lock);
    for (size_t i = 0; i < command_count; i++) {
        if (strcmp(commands[i].name, path + length + 1) == 0)
            context = commands[i].context;
    }
    unlock(&commands_lock);
    if (context != NULL)
        strlcpy(name, path + length + 1, MAX_NAME);
    return context;
}

void swipterm_invocation_release(swipterm_invocation *invocation) {
    if (atomic_fetch_sub(&invocation->references, 1) == 1)
        free(invocation);
}

void swipterm_invocation_finish(swipterm_invocation *invocation, int status) {
    lock(&invocations_lock);
    if (!invocation->ended)
        (void) write(invocation->done_fd, &status, sizeof(status));
    unlock(&invocations_lock);
}

static void deliver_abort(void *context) {
    callbacks.command_abort(context);
}

static void deliver_release(void *context) {
    callbacks.command_release(context);
}

void commands_signal(struct task *task, int sig) {
    if (sig != SIGINT_ && sig != SIGTERM_ && sig != SIGKILL_ && sig != SIGHUP_ && sig != SIGQUIT_)
        return;
    if (sig != SIGKILL_ && sigset_has(task->blocked, sig))
        return;
    lock(&invocations_lock);
    swipterm_invocation *invocation;
    list_for_each_entry(&invocations, invocation, link) {
        if (invocation->tgid == task->tgid && !invocation->aborted) {
            invocation->aborted = true;
            if (invocation->context != NULL)
                dispatch_async_f(events_queue, invocation->context, deliver_abort);
        }
    }
    unlock(&invocations_lock);
}

static struct fd *host_pipe_fd(int real_fd) {
    fcntl(real_fd, F_SETFL, fcntl(real_fd, F_GETFL) | O_NONBLOCK);
    struct fd *fd = adhoc_fd_create(&host_pipe_fdops);
    if (fd == NULL) {
        close(real_fd);
        return NULL;
    }
    fd->real_fd = real_fd;
    return fd;
}

static void apply_exec_semantics(const char *name) {
    lock(&current->general_lock);
    strncpy(current->comm, name, sizeof(current->comm));
    unlock(&current->general_lock);
    update_thread_name();
    fdtable_do_cloexec(current->files);
    lock(&current->sighand->lock);
    for (int sig = 0; sig < NUM_SIGS; sig++) {
        struct sigaction_ *action = &current->sighand->action[sig];
        if (action->handler != SIG_IGN_)
            action->handler = SIG_DFL_;
    }
    current->sighand->altstack = 0;
    unlock(&current->sighand->lock);
    current->did_exec = true;
    current->undumpable = false;
    vfork_notify(current);
}

static char **unpack_strings(const char *packed, size_t count) {
    char **strings = calloc(count + 1, sizeof(char *));
    if (strings == NULL)
        return NULL;
    const char *string = packed;
    for (size_t i = 0; i < count; i++, string += strlen(string) + 1)
        strings[i] = (char *) string;
    return strings;
}

static void close_channel(struct channel *channel) {
    channel->ended = true;
    channel->length = 0;
    if (channel->host_source && channel->source != NULL) {
        fd_close(channel->source);
        channel->source = NULL;
    } else if (!channel->host_source && channel->sink != NULL) {
        fd_close(channel->sink);
        channel->sink = NULL;
    }
}

static void session_cleanup(void *context) {
    struct session *session = context;
    swipterm_invocation *invocation = session->invocation;
    lock(&invocations_lock);
    list_remove(&invocation->link);
    invocation->ended = true;
    void *swift_context = invocation->context;
    unlock(&invocations_lock);
    for (int i = 0; i < 3; i++) {
        struct channel *channel = &session->channels[i];
        if (channel->source != NULL)
            fd_close(channel->source);
        if (channel->sink != NULL)
            fd_close(channel->sink);
        free(channel->buffer);
    }
    if (session->done != NULL)
        fd_close(session->done);
    close(session->done_fds[1]);
    free(session->argv);
    free(session->envp);
    if (swift_context != NULL)
        dispatch_async_f(events_queue, swift_context, deliver_release);
    swipterm_invocation_release(invocation);
}

static void channel_read(struct channel *channel) {
    ssize_t count;
    if (channel->host_source) {
        count = read(channel->source->real_fd, channel->buffer, CHANNEL_BUFFER_SIZE);
        if (count < 0)
            count = errno == EAGAIN || errno == EINTR ? _EAGAIN : _EIO;
    } else if (channel->source->ops->read != NULL) {
        count = channel->source->ops->read(channel->source, channel->buffer, CHANNEL_BUFFER_SIZE);
    } else {
        count = _EBADF;
    }
    if (count > 0) {
        channel->offset = 0;
        channel->length = count;
    } else if (count != _EAGAIN && count != _EINTR) {
        channel->ended = true;
        if (!channel->host_source)
            close_channel(channel);
    }
}

static void channel_write(struct channel *channel) {
    ssize_t count;
    if (channel->sink == NULL) {
        count = channel->length - channel->offset;
    } else if (!channel->host_source) {
        count = write(channel->sink->real_fd, channel->buffer + channel->offset, channel->length - channel->offset);
        if (count < 0)
            count = errno == EAGAIN || errno == EINTR ? _EAGAIN : _EPIPE;
    } else if (channel->sink->ops->write != NULL) {
        count = channel->sink->ops->write(channel->sink, channel->buffer + channel->offset,
                                          channel->length - channel->offset);
    } else {
        count = _EBADF;
    }
    if (count >= 0) {
        channel->offset += count;
        if (channel->offset == channel->length)
            channel->offset = channel->length = 0;
    } else if (count != _EAGAIN && count != _EINTR) {
        close_channel(channel);
    }
}

#define EVENT_READ 0
#define EVENT_WRITE 1
#define EVENT_DONE 6

static int poll_event(void *context, int types, union poll_fd_info info) {
    int *ready = context;
    *ready |= 1 << info.num;
    return 1;
}

static bool channel_finished(struct channel *channel) {
    return channel->ended && channel->length == 0;
}

static void watch(struct poll *poll, struct fd *fd, int types, int event, int *count, int *immediate) {
    if (fd->ops->poll == NULL)
        *immediate |= 1 << event;
    else
        poll_add_fd(poll, fd, (*count)++, types, (union poll_fd_info) {.num = event});
}

static void run_session(struct session *session) {
    for (;;) {
        if (session->finished && channel_finished(&session->channels[1]) && channel_finished(&session->channels[2]))
            return;
        struct poll *poll = poll_create();
        if (IS_ERR(poll))
            return;
        int count = 0;
        int ready = 0;
        for (int i = 0; i < 3; i++) {
            struct channel *channel = &session->channels[i];
            if (channel->length > 0 && channel->sink != NULL)
                watch(poll, channel->sink, POLL_WRITE, i * 2 + EVENT_WRITE, &count, &ready);
            else if (channel->length > 0)
                ready |= 1 << (i * 2 + EVENT_WRITE);
            else if (!channel->ended && channel->source != NULL)
                watch(poll, channel->source, POLL_READ, i * 2 + EVENT_READ, &count, &ready);
        }
        if (!session->finished)
            watch(poll, session->done, POLL_READ, EVENT_DONE, &count, &ready);
        int err = 0;
        if (ready == 0 && count > 0)
            err = poll_wait(poll, poll_event, &ready, NULL);
        poll_destroy(poll);
        if (err == _EINTR) {
            receive_signals();
            continue;
        }
        for (int i = 0; i < 3; i++) {
            if (ready & (1 << (i * 2 + EVENT_READ)))
                channel_read(&session->channels[i]);
            if (ready & (1 << (i * 2 + EVENT_WRITE)))
                channel_write(&session->channels[i]);
        }
        if (ready & (1 << EVENT_DONE)) {
            int status;
            if (read(session->done->real_fd, &status, sizeof(status)) == sizeof(status)) {
                session->finished = true;
                session->status = status;
            }
        }
        struct channel *input = &session->channels[0];
        if (input->ended && input->length == 0 && input->sink != NULL) {
            fd_close(input->sink);
            input->sink = NULL;
        }
    }
}

static int create_session(struct session *session, size_t argc, const char *argv, const char *envp) {
    int pipes[4][2];
    int created = 0;
    for (; created < 4; created++) {
        if (pipe(pipes[created]) < 0)
            break;
        fcntl(pipes[created][0], F_SETFD, FD_CLOEXEC);
        fcntl(pipes[created][1], F_SETFD, FD_CLOEXEC);
    }
    size_t envc = 0;
    for (const char *string = envp; *string != '\0'; string += strlen(string) + 1)
        envc++;
    session->argc = argc;
    session->argv = unpack_strings(argv, argc);
    session->envp = unpack_strings(envp, envc);
    session->invocation = calloc(1, sizeof(*session->invocation));
    bool allocated = session->argv != NULL && session->envp != NULL && session->invocation != NULL;
    for (int i = 0; i < 3; i++) {
        session->channels[i].buffer = malloc(CHANNEL_BUFFER_SIZE);
        allocated &= session->channels[i].buffer != NULL;
    }
    if (created == 4 && allocated) {
        session->host_fds[0] = pipes[0][0];
        session->host_fds[1] = pipes[1][1];
        session->host_fds[2] = pipes[2][1];
        session->channels[0].sink = host_pipe_fd(pipes[0][1]);
        session->channels[1].source = host_pipe_fd(pipes[1][0]);
        session->channels[2].source = host_pipe_fd(pipes[2][0]);
        session->done = host_pipe_fd(pipes[3][0]);
        session->done_fds[1] = pipes[3][1];
        if (session->channels[0].sink != NULL && session->channels[1].source != NULL &&
            session->channels[2].source != NULL && session->done != NULL)
            return 0;
        for (int i = 0; i < 3; i++)
            close(session->host_fds[i]);
        close(session->done_fds[1]);
        created = 0;
    }
    int err = created < 4 ? errno_map() : _ENOMEM;
    for (int i = 0; i < created; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    for (int i = 0; i < 3; i++) {
        if (session->channels[i].sink != NULL)
            fd_close(session->channels[i].sink);
        if (session->channels[i].source != NULL)
            fd_close(session->channels[i].source);
        free(session->channels[i].buffer);
    }
    if (session->done != NULL)
        fd_close(session->done);
    free(session->argv);
    free(session->envp);
    free(session->invocation);
    return err;
}

static void start_session(struct session *session, void *command_context) {
    session->channels[1].host_source = true;
    session->channels[2].host_source = true;
    lock(&current->files->lock);
    for (int i = 0; i < 3; i++) {
        struct fd *fd = current->files->size > (unsigned) i ? current->files->files[i] : NULL;
        if (fd != NULL)
            fd_retain(fd);
        if (i == 0)
            session->channels[i].source = fd;
        else
            session->channels[i].sink = fd;
    }
    unlock(&current->files->lock);
    if (session->channels[0].source == NULL)
        session->channels[0].ended = true;

    char working_directory[MAX_PATH];
    lock(&current->fs->lock);
    if (generic_getpath(current->fs->pwd, working_directory) < 0)
        strcpy(working_directory, "/");
    unlock(&current->fs->lock);

    swipterm_invocation *invocation = session->invocation;
    atomic_init(&invocation->references, 2);
    invocation->tgid = current->tgid;
    invocation->done_fd = session->done_fds[1];
    lock(&invocations_lock);
    list_add(&invocations, &invocation->link);
    unlock(&invocations_lock);

    size_t argc = session->argc;
    void *context = callbacks.command_start(command_context, invocation, argc > 0 ? argc - 1 : 0,
                                            (const char *const *) session->argv + (argc > 0 ? 1 : 0),
                                            (const char *const *) session->envp, working_directory,
                                            session->host_fds[0], session->host_fds[1], session->host_fds[2]);
    lock(&invocations_lock);
    invocation->context = context;
    bool aborted = invocation->aborted;
    unlock(&invocations_lock);
    if (aborted)
        dispatch_async_f(events_queue, context, deliver_abort);
}

int commands_exec(const char *file, size_t argc, const char *argv, const char *envp) {
    char name[MAX_NAME];
    void *command_context = find_command(file, name);
    if (command_context == NULL)
        return 0;
    int err = process_start_pending();
    if (err < 0)
        return err;
    struct session session = {.done_fds = {-1, -1}};
    err = create_session(&session, argc, argv, envp);
    if (err < 0)
        return err;
    apply_exec_semantics(name);
    pthread_cleanup_push(session_cleanup, &session);
    start_session(&session, command_context);
    run_session(&session);
    pthread_cleanup_pop(1);
    receive_signals();
    do_exit_group((session.status & 0xff) << 8);
}
