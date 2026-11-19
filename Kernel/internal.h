#ifndef SWIPTERM_INTERNAL_H
#define SWIPTERM_INTERNAL_H

#include <dispatch/dispatch.h>

#include "kernel/calls.h"
#include "kernel/task.h"
#include "fs/fd.h"
#include "fs/path.h"

#include "swipterm.h"

extern swipterm_callbacks callbacks;
extern dispatch_queue_t events_queue;
extern struct task *init_task;
extern char *root_path;

void host_enter(void);
void host_leave(void);

bool signal_pending(void);
int wait_real_fd(int real_fd, short events);
struct fd *pipe_fd_create(int real_fd, int flags);

void processes_boot(void);
void process_start_reaper(void);
void process_task_exited(struct task *task, int code);
int process_start_pending(void);
int process_start_init(const char *path, size_t argc, const char *const *argv, const char *const *envp);

int terminal_bind(swipterm_terminal *terminal);
void terminal_unbind(swipterm_terminal *terminal);
struct fd *terminal_open_slave(swipterm_terminal *terminal);

int filesystem_boot(void);
int filesystem_write_file(const char *path, const void *data, size_t length, uint32_t mode);
int filesystem_read_file(const char *path, void **data_out, size_t *length_out);
int filesystem_create_directory(const char *path, uint32_t mode, bool intermediates);

int commands_boot(const char *directory);
int commands_exec(const char *file, size_t argc, const char *argv, const char *envp);
void commands_signal(struct task *task, int sig);

#endif
