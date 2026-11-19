#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "internal.h"

#include "kernel/init.h"
#include "fs/devices.h"
#include "fs/real.h"
#include "fs/tty.h"

#define P_ALL_ 0
#define WNOHANG_ 1
#define WEXITED_ 4
int do_wait(int idtype, pid_t_ id, struct siginfo_ *info, struct rusage_ *rusage, int options);

struct swipterm_process {
    atomic_int references;
    pid_t_ pid;
    bool exited;
    int status;
    void *context;
    struct list link;
};

struct start {
    struct task *task;
    pid_t_ pid;
    const char *path;
    size_t argc;
    char *argv;
    char *envp;
    const char *working_directory;
    bool install_stdio;
    swipterm_stdio stdio[3];
    int guest_fds[3];
    bool terminal_opened;
    swipterm_process *process;
    lock_t lock;
    cond_t cond;
    bool done;
    int err;
};

static lock_t processes_lock = LOCK_INITIALIZER;
static struct list processes = LIST_INITIALIZER(processes);
static struct fd_ops pipe_fdops;
static pthread_attr_t detached_attr;
static __thread struct start *pending_start;

static ssize_t pipe_read(struct fd *fd, void *buf, size_t size) {
    for (;;) {
        ssize_t count = read(fd->real_fd, buf, size);
        if (count >= 0)
            return count;
        if (errno == EINTR)
            continue;
        if (errno != EAGAIN)
            return errno_map();
        if (fd->flags & O_NONBLOCK_)
            return _EAGAIN;
        int err = wait_real_fd(fd->real_fd, POLLIN);
        if (err < 0)
            return err;
    }
}

static ssize_t pipe_write(struct fd *fd, const void *buf, size_t size) {
    size_t written = 0;
    while (written < size) {
        ssize_t count = write(fd->real_fd, (const char *) buf + written, size - written);
        if (count >= 0) {
            written += count;
            continue;
        }
        if (errno == EINTR)
            continue;
        int err;
        if (errno != EAGAIN)
            err = errno_map();
        else if (fd->flags & O_NONBLOCK_)
            err = _EAGAIN;
        else
            err = wait_real_fd(fd->real_fd, POLLOUT);
        if (err < 0)
            return written > 0 ? (ssize_t) written : err;
    }
    return written;
}

static int pipe_getflags(struct fd *fd) {
    return fd->flags;
}

static int pipe_setflags(struct fd *fd, dword_t flags) {
    fd->flags = (fd->flags & ~(O_APPEND_ | O_NONBLOCK_)) | (flags & (O_APPEND_ | O_NONBLOCK_));
    return 0;
}

struct fd *pipe_fd_create(int real_fd, int flags) {
    struct stat real_stat;
    if (fstat(real_fd, &real_stat) < 0 || fcntl(real_fd, F_SETFL, fcntl(real_fd, F_GETFL) | O_NONBLOCK) < 0)
        return ERR_PTR(errno_map());
    struct fd *fd = adhoc_fd_create(&pipe_fdops);
    if (fd == NULL)
        return ERR_PTR(_ENOMEM);
    fd->real_fd = real_fd;
    fd->flags = flags;
    fd->type = real_stat.st_mode & S_IFMT;
    fd->stat.mode = real_stat.st_mode;
    fd->stat.inode = real_stat.st_ino;
    fd->stat.nlink = real_stat.st_nlink;
    fd->stat.blksize = real_stat.st_blksize;
    return fd;
}

void swipterm_process_release(swipterm_process *process) {
    if (atomic_fetch_sub(&process->references, 1) == 1)
        free(process);
}

static void deliver_exit(void *context) {
    swipterm_process *process = context;
    callbacks.process_exit(process->context, process->status);
    swipterm_process_release(process);
}

void process_task_exited(struct task *task, int code) {
    struct tgroup *group = task->group;
    int status = group->doing_group_exit ? group->group_exit_code : group->leader->exit_code;
    swipterm_process *process, *exited = NULL;
    lock(&processes_lock);
    list_for_each_entry(&processes, process, link) {
        if (process->pid == task->tgid) {
            exited = process;
            break;
        }
    }
    if (exited != NULL) {
        list_remove(&exited->link);
        exited->exited = true;
        exited->status = status;
    }
    unlock(&processes_lock);
    if (exited != NULL)
        dispatch_async_f(events_queue, exited, deliver_exit);
}

int swipterm_process_signal(swipterm_process *process, int signal, bool group) {
    if (signal < 0 || signal >= NUM_SIGS)
        return _EINVAL;
    struct siginfo_ info = {.code = SI_USER_};
    int err = _ESRCH;
    lock(&pids_lock);
    lock(&processes_lock);
    bool exited = process->exited;
    unlock(&processes_lock);
    struct task *task = exited ? NULL : pid_get_task(process->pid);
    if (task != NULL) {
        err = 0;
        if (!group) {
            send_signal(task, signal, info);
        } else {
            struct pid *pgid = pid_get(task->group->pgid);
            struct tgroup *member;
            list_for_each_entry(&pgid->pgroup, member, pgroup) {
                send_signal(member->leader, signal, info);
            }
        }
    }
    unlock(&pids_lock);
    return err;
}

static struct fd *open_null(void) {
    struct fd *fd = generic_open("/dev/null", O_RDWR_, 0);
    if (!IS_ERR(fd) && S_ISCHR(fd->stat.mode))
        return fd;
    if (!IS_ERR(fd))
        fd_close(fd);
    fd = adhoc_fd_create(NULL);
    if (fd == NULL)
        return ERR_PTR(_ENOMEM);
    fd->real_fd = -1;
    fd->type = S_IFCHR;
    fd->stat.mode = S_IFCHR | 0666;
    fd->stat.rdev = dev_make(MEM_MAJOR, DEV_NULL_MINOR);
    fd->flags = O_RDWR_;
    int err = dev_open(MEM_MAJOR, DEV_NULL_MINOR, DEV_CHAR, fd);
    if (err < 0) {
        fd_close(fd);
        return ERR_PTR(err);
    }
    return fd;
}

static int install_stdio(struct start *start) {
    struct fd *fds[3] = {NULL, NULL, NULL};
    int err = 0;
    for (int i = 0; i < 3 && err == 0; i++) {
        swipterm_stdio *stdio = &start->stdio[i];
        switch (stdio->kind) {
            case SWIPTERM_STDIO_NULL:
                fds[i] = open_null();
                break;
            case SWIPTERM_STDIO_PIPE:
                fds[i] = pipe_fd_create(start->guest_fds[i], i == 0 ? O_RDONLY_ : O_WRONLY_);
                if (!IS_ERR(fds[i]))
                    start->guest_fds[i] = -1;
                break;
            case SWIPTERM_STDIO_TERMINAL:
                for (int j = 0; j < i; j++) {
                    if (start->stdio[j].kind == SWIPTERM_STDIO_TERMINAL && start->stdio[j].terminal == stdio->terminal) {
                        fds[i] = fd_retain(fds[j]);
                        break;
                    }
                }
                if (fds[i] == NULL) {
                    start->terminal_opened = true;
                    fds[i] = terminal_open_slave(stdio->terminal);
                }
                break;
        }
        if (IS_ERR(fds[i])) {
            err = (int) PTR_ERR(fds[i]);
            fds[i] = NULL;
        }
    }
    if (err < 0) {
        for (int i = 0; i < 3; i++) {
            if (fds[i] != NULL)
                fd_close(fds[i]);
        }
        return err;
    }
    for (int i = 0; i < 3; i++)
        current->files->files[i] = fds[i];
    return 0;
}

static void finish_start(struct start *start, int err) {
    if (err == 0 && start->process != NULL) {
        start->process->pid = start->pid;
        lock(&processes_lock);
        list_add(&processes, &start->process->link);
        unlock(&processes_lock);
    }
    lock(&start->lock);
    start->err = err;
    start->done = true;
    notify(&start->cond);
    unlock(&start->lock);
}

int process_start_pending(void) {
    struct start *start = pending_start;
    if (start == NULL)
        return 0;
    pending_start = NULL;
    int err = start->install_stdio ? install_stdio(start) : 0;
    finish_start(start, err);
    return err;
}

static int change_directory(const char *path) {
    struct statbuf stat;
    int err = generic_statat(AT_PWD, path, &stat, true);
    if (err < 0)
        return err;
    if (!S_ISDIR(stat.mode))
        return _ENOTDIR;
    struct fd *dir = generic_open(path, O_RDONLY_, 0);
    if (IS_ERR(dir))
        return (int) PTR_ERR(dir);
    fs_chdir(current->fs, dir);
    return 0;
}

static void *start_thread(void *context) {
    struct start *start = context;
    // Dispatch worker threads block SIGUSR1, and the threads of guest tasks inherit the mask of
    // the thread that spawned their first ancestor.
    sigset_t interrupt;
    sigemptyset(&interrupt);
    sigaddset(&interrupt, SIGUSR1);
    pthread_sigmask(SIG_UNBLOCK, &interrupt, NULL);
    int err = 0;
    if (start->task != NULL) {
        current = start->task;
    } else {
        err = become_new_init_child();
        if (err < 0) {
            finish_start(start, err);
            return NULL;
        }
    }
    current->thread = pthread_self();
    start->pid = current->pid;
    pending_start = start;
    char *argv = start->argv;
    char *envp = start->envp;
    if (start->working_directory != NULL)
        err = change_directory(start->working_directory);
    if (err == 0) {
        err = execve_packed(start->path, start->argc, argv, envp);
    } else {
        free(argv);
        free(envp);
    }
    if (pending_start != NULL) {
        if (err == 0) {
            err = process_start_pending();
        } else {
            pending_start = NULL;
            finish_start(start, err);
        }
    }
    if (err < 0)
        do_exit_group(127 << 8);
    update_thread_name();
    task_run_current();
    return NULL;
}

static int pack_strings(char **buffer_out, size_t count, const char *const *strings) {
    char *buffer = malloc(ARGV_MAX);
    if (buffer == NULL)
        return _ENOMEM;
    size_t used = 0;
    for (size_t i = 0; i < count; i++) {
        size_t length = strlen(strings[i]) + 1;
        if (used + length + 1 > ARGV_MAX) {
            free(buffer);
            return _E2BIG;
        }
        memcpy(buffer + used, strings[i], length);
        used += length;
    }
    buffer[used] = '\0';
    *buffer_out = buffer;
    return 0;
}

static size_t count_strings(const char *const *strings) {
    size_t count = 0;
    while (strings[count] != NULL)
        count++;
    return count;
}

static int run_start(struct start *start) {
    lock_init(&start->lock);
    cond_init(&start->cond);
    pthread_t thread;
    int err = pthread_create(&thread, &detached_attr, start_thread, start);
    if (err != 0) {
        free(start->argv);
        free(start->envp);
        return err == EAGAIN ? _EAGAIN : _ENOMEM;
    }
    lock(&start->lock);
    while (!start->done)
        wait_for_ignore_signals(&start->cond, &start->lock, NULL);
    unlock(&start->lock);
    cond_destroy(&start->cond);
    return start->err;
}

static int prepare_start(struct start *start, const char *path, size_t argc, const char *const *argv,
                         const char *const *envp) {
    start->path = path;
    start->argc = argc;
    for (int i = 0; i < 3; i++)
        start->guest_fds[i] = -1;
    int err = pack_strings(&start->argv, argc, argv);
    if (err < 0)
        return err;
    err = pack_strings(&start->envp, count_strings(envp), envp);
    if (err < 0) {
        free(start->argv);
        return err;
    }
    return 0;
}

int swipterm_spawn(const swipterm_spawn_options *options, swipterm_process **process_out,
                   int *pid_out, int host_fds_out[3]) {
    if (init_task->exiting)
        return _ESRCH;
    if (options->argc == 0)
        return _EINVAL;
    struct start start = {.working_directory = options->working_directory, .install_stdio = true};
    int err = prepare_start(&start, options->path, options->argc, options->argv, options->envp);
    if (err < 0)
        return err;

    int host_fds[3] = {-1, -1, -1};
    int bound = 0;
    for (int i = 0; i < 3 && err == 0; i++) {
        start.stdio[i] = options->stdio[i];
        if (options->stdio[i].kind == SWIPTERM_STDIO_PIPE) {
            int fds[2];
            if (pipe(fds) < 0) {
                err = errno_map();
                break;
            }
            fcntl(fds[0], F_SETFD, FD_CLOEXEC);
            fcntl(fds[1], F_SETFD, FD_CLOEXEC);
            start.guest_fds[i] = fds[i == 0 ? 0 : 1];
            host_fds[i] = fds[i == 0 ? 1 : 0];
        } else if (options->stdio[i].kind == SWIPTERM_STDIO_TERMINAL) {
            bool seen = false;
            for (int j = 0; j < i; j++)
                seen |= options->stdio[j].kind == SWIPTERM_STDIO_TERMINAL && options->stdio[j].terminal == options->stdio[i].terminal;
            if (!seen) {
                err = terminal_bind(options->stdio[i].terminal);
                if (err == 0)
                    bound |= 1 << i;
            }
        }
    }

    swipterm_process *process = NULL;
    if (err == 0) {
        process = calloc(1, sizeof(*process));
        if (process == NULL) {
            err = _ENOMEM;
        } else {
            atomic_init(&process->references, 2);
            process->context = options->context;
            start.process = process;
        }
    }
    if (err == 0)
        err = run_start(&start);
    else {
        free(start.argv);
        free(start.envp);
    }

    for (int i = 0; i < 3; i++) {
        if (start.guest_fds[i] >= 0)
            close(start.guest_fds[i]);
    }
    if (err < 0) {
        for (int i = 0; i < 3; i++) {
            if (host_fds[i] >= 0)
                close(host_fds[i]);
            if ((bound & (1 << i)) && !start.terminal_opened)
                terminal_unbind(options->stdio[i].terminal);
        }
        free(process);
        return err;
    }
    *process_out = process;
    *pid_out = start.pid;
    for (int i = 0; i < 3; i++)
        host_fds_out[i] = host_fds[i];
    return 0;
}

int process_start_init(const char *path, size_t argc, const char *const *argv, const char *const *envp) {
    struct start start = {.task = init_task};
    int err = prepare_start(&start, path, argc, argv, envp);
    if (err < 0)
        return err;
    return run_start(&start);
}

static void *reaper_thread(void *context) {
    current = init_task;
    lock(&pids_lock);
    for (;;) {
        bool zombie = false;
        struct task *child;
        list_for_each_entry(&init_task->children, child, siblings) {
            if (child->zombie) {
                zombie = true;
                break;
            }
        }
        if (!zombie) {
            wait_for_ignore_signals(&init_task->group->child_exit, &pids_lock, NULL);
            continue;
        }
        unlock(&pids_lock);
        struct siginfo_ info;
        while (do_wait(P_ALL_, 0, &info, NULL, WEXITED_ | WNOHANG_) == 0 && info.child.pid != 0)
            ;
        lock(&pids_lock);
    }
    return NULL;
}

void processes_boot(void) {
    pthread_attr_init(&detached_attr);
    pthread_attr_setdetachstate(&detached_attr, PTHREAD_CREATE_DETACHED);
    pipe_fdops = realfs_fdops;
    pipe_fdops.read = pipe_read;
    pipe_fdops.write = pipe_write;
    pipe_fdops.pread = NULL;
    pipe_fdops.pwrite = NULL;
    pipe_fdops.readdir = NULL;
    pipe_fdops.telldir = NULL;
    pipe_fdops.seekdir = NULL;
    pipe_fdops.mmap = NULL;
    pipe_fdops.getflags = pipe_getflags;
    pipe_fdops.setflags = pipe_setflags;
}

void process_start_reaper(void) {
    strncpy(init_task->comm, "init", sizeof(init_task->comm));
    pthread_create(&init_task->thread, &detached_attr, reaper_thread, NULL);
}
