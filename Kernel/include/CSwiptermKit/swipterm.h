#ifndef SWIPTERM_H
#define SWIPTERM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

typedef struct swipterm_process swipterm_process;
typedef struct swipterm_terminal swipterm_terminal;
typedef struct swipterm_invocation swipterm_invocation;

typedef void (*swipterm_progress_handler)(void *context, double fraction);

bool swipterm_image_import_file(const char *archive_path, const char *root_path,
                                void *context, swipterm_progress_handler progress, char **error_message);
bool swipterm_image_import_memory(const void *data, size_t size, const char *root_path,
                                  void *context, swipterm_progress_handler progress, char **error_message);
bool swipterm_image_export(const char *root_path, const char *archive_path,
                           void *context, swipterm_progress_handler progress, char **error_message);

typedef struct {
    void (*console_output)(const void *data, size_t length);
    void (*halt)(void);
    void (*panic)(const char *message);
    void (*process_exit)(void *context, int status);
    void *(*command_start)(void *context, swipterm_invocation *invocation,
                           size_t argc, const char *const *argv, const char *const *envp,
                           const char *working_directory, int input_fd, int output_fd, int error_fd);
    void (*command_abort)(void *context);
    void (*command_release)(void *context);
    void *(*device_open)(void *context, int flags, int *error_out);
    ssize_t (*device_read)(void *file, void *buffer, size_t length);
    ssize_t (*device_write)(void *file, const void *buffer, size_t length);
    void (*device_close)(void *file);
} swipterm_callbacks;

typedef struct {
    const char *root_path;
    const char *socket_prefix;
    const char *hostname;
    const char *version;
    const char *host_commands_directory;
    swipterm_callbacks callbacks;
} swipterm_options;

int swipterm_boot(const swipterm_options *options);
int swipterm_start_init(const char *path, size_t argc, const char *const *argv, const char *const *envp);
void swipterm_suspend(void);
void swipterm_resume(void);
int swipterm_sync_dns(void);

typedef enum {
    SWIPTERM_STDIO_NULL,
    SWIPTERM_STDIO_PIPE,
    SWIPTERM_STDIO_TERMINAL,
} swipterm_stdio_kind;

typedef struct {
    swipterm_stdio_kind kind;
    swipterm_terminal *terminal;
} swipterm_stdio;

typedef struct {
    const char *path;
    size_t argc;
    const char *const *argv;
    const char *const *envp;
    const char *working_directory;
    swipterm_stdio stdio[3];
    void *context;
} swipterm_spawn_options;

int swipterm_spawn(const swipterm_spawn_options *options, swipterm_process **process_out,
                   int *pid_out, int host_fds_out[3]);
int swipterm_process_signal(swipterm_process *process, int signal, bool group);
void swipterm_process_release(swipterm_process *process);

swipterm_terminal *swipterm_terminal_create(unsigned short columns, unsigned short rows, int *error_out);
int swipterm_terminal_notify_fd(swipterm_terminal *terminal);
ssize_t swipterm_terminal_read(swipterm_terminal *terminal, void *buffer, size_t length);
int swipterm_terminal_write(swipterm_terminal *terminal, const void *data, size_t length);
void swipterm_terminal_resize(swipterm_terminal *terminal, unsigned short columns, unsigned short rows);
void swipterm_terminal_close(swipterm_terminal *terminal);
void swipterm_terminal_release(swipterm_terminal *terminal);

typedef struct {
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t nlink;
    uint64_t size;
    uint64_t inode;
    uint64_t rdev;
    struct timespec access_time;
    struct timespec modification_time;
    struct timespec change_time;
} swipterm_stat;

typedef struct {
    char *name;
    uint32_t mode;
} swipterm_directory_entry;

int swipterm_stat_path(const char *path, bool follow_symlinks, swipterm_stat *stat_out);
int swipterm_read_file(const char *path, void **data_out, size_t *length_out);
int swipterm_write_file(const char *path, const void *data, size_t length, uint32_t mode);
int swipterm_list_directory(const char *path, swipterm_directory_entry **entries_out, size_t *count_out);
void swipterm_free_directory_entries(swipterm_directory_entry *entries, size_t count);
int swipterm_create_directory(const char *path, uint32_t mode, bool intermediates);
int swipterm_remove(const char *path, bool recursive);
int swipterm_mount(const char *host_path, const char *guest_path, bool read_only);
int swipterm_unmount(const char *guest_path);

int swipterm_register_command(const char *name, void *context);
void swipterm_invocation_finish(swipterm_invocation *invocation, int status);
void swipterm_invocation_release(swipterm_invocation *invocation);

int swipterm_register_device(const char *path, void *context);

#endif
