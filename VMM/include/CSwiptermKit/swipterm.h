#ifndef SWIPTERM_H
#define SWIPTERM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

typedef struct swipterm_runtime swipterm_runtime;
typedef struct swipterm_process swipterm_process;
typedef struct swipterm_terminal swipterm_terminal;
typedef struct swipterm_invocation swipterm_invocation;
typedef struct swipterm_network swipterm_network;

typedef bool (*swipterm_progress_handler)(void *context, double fraction);

bool swipterm_root_import_file(const char *archive_path, const char *root_path, uint64_t disk_size,
                               void *context, swipterm_progress_handler progress, char **error_message);
bool swipterm_root_import_memory(const void *data, size_t size, const char *root_path, uint64_t disk_size,
                                 void *context, swipterm_progress_handler progress, char **error_message);
bool swipterm_root_export(const char *root_path, const char *archive_path,
                          void *context, swipterm_progress_handler progress, char **error_message);

typedef enum {
    SWIPTERM_HALT_NONE = 0,
    SWIPTERM_HALT_EXIT = 1,
    SWIPTERM_HALT_REBOOT = 2,
    SWIPTERM_HALT_POWER_OFF = 3,
} swipterm_halt_reason;

typedef struct {
    void *context;
    void (*console_output)(void *context, const void *data, size_t length);
    void (*halt)(void *context, swipterm_halt_reason reason, int wait_status);
    void (*panic)(void *context, const char *message);
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

typedef enum {
    SWIPTERM_KERNEL_INSTALLED = 0,
    SWIPTERM_KERNEL_BUNDLED = 1,
    SWIPTERM_KERNEL_RELEASE = 2,
} swipterm_kernel_selection;

typedef struct {
    const char *root_path;
    const char *hostname;
    const char *host_commands_directory;
    size_t ram_size;
    uint32_t cpu_count;
    swipterm_callbacks callbacks;
    const char *bundled_kernel_path;
    const char *initramfs_path;
    swipterm_kernel_selection kernel;
    const char *kernel_release;
    const char *extra_cmdline;
    swipterm_network *network;
} swipterm_options;

typedef struct {
    char release[65];
    bool bundled;
    char fallback_reason[256];
} swipterm_boot_info;

int swipterm_boot(const swipterm_options *options, swipterm_runtime **runtime_out);
void swipterm_begin_shutdown(swipterm_runtime *runtime);
void swipterm_shutdown(swipterm_runtime *runtime);
int swipterm_start_init(swipterm_runtime *runtime, const char *path,
                        size_t argc, const char *const *argv, const char *const *envp);
void swipterm_suspend(swipterm_runtime *runtime);
void swipterm_resume(swipterm_runtime *runtime);
int swipterm_sync_dns(swipterm_runtime *runtime);
void swipterm_get_boot_info(swipterm_runtime *runtime, swipterm_boot_info *info_out);

typedef enum {
    SWIPTERM_READY_LOGIN = 1,
    SWIPTERM_READY_SYSTEM = 2,
} swipterm_readiness;

int swipterm_wait_ready(swipterm_runtime *runtime, swipterm_readiness readiness, int timeout_ms);

typedef struct {
    swipterm_halt_reason reason;
    int wait_status;
    bool shutdown_complete;
    bool forced;
    bool storage_error;
} swipterm_halt_info;

void swipterm_get_halt_info(swipterm_runtime *runtime, swipterm_halt_info *info_out);

typedef enum {
    SWIPTERM_STDIO_NULL = 0,
    SWIPTERM_STDIO_PIPE = 1,
    SWIPTERM_STDIO_TERMINAL = 2,
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

int swipterm_spawn(swipterm_runtime *runtime, const swipterm_spawn_options *options,
                   swipterm_process **process_out, int *pid_out, int host_fds_out[3]);
typedef struct {
    swipterm_terminal *terminal;
    const char *user;
    const char *const *envp;
    void *context;
} swipterm_login_options;

int swipterm_login(swipterm_runtime *runtime, const swipterm_login_options *options,
                   swipterm_process **process_out, int *pid_out);
int swipterm_process_signal(swipterm_process *process, int signal, bool group);
void swipterm_process_release(swipterm_process *process);

swipterm_terminal *swipterm_terminal_create(swipterm_runtime *runtime,
                                            unsigned short columns, unsigned short rows, int *error_out);
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

int swipterm_stat_path(swipterm_runtime *runtime, const char *path,
                       bool follow_symlinks, swipterm_stat *stat_out);
int swipterm_read_file(swipterm_runtime *runtime, const char *path, void **data_out, size_t *length_out);
int swipterm_write_file(swipterm_runtime *runtime, const char *path,
                        const void *data, size_t length, uint32_t mode);
int swipterm_list_directory(swipterm_runtime *runtime, const char *path,
                            swipterm_directory_entry **entries_out, size_t *count_out);
void swipterm_free_directory_entries(swipterm_directory_entry *entries, size_t count);
int swipterm_create_directory(swipterm_runtime *runtime, const char *path, uint32_t mode, bool intermediates);
int swipterm_remove(swipterm_runtime *runtime, const char *path, bool recursive);
int swipterm_mount(swipterm_runtime *runtime, const char *host_path, const char *guest_path, bool read_only);
int swipterm_unmount(swipterm_runtime *runtime, const char *guest_path);

int swipterm_register_command(swipterm_runtime *runtime, const char *name, void *context);
void swipterm_invocation_finish(swipterm_invocation *invocation, int status);
void swipterm_invocation_release(swipterm_invocation *invocation);

int swipterm_register_device(swipterm_runtime *runtime, const char *path, void *context);

#endif
