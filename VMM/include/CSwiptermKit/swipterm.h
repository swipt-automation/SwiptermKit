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

typedef struct swipterm_root_builder swipterm_root_builder;
typedef struct swipterm_archive swipterm_archive;

typedef bool (*swipterm_progress_handler)(void *context, double fraction);

typedef struct {
    void *context;
    swipterm_progress_handler progress;
    void (*metadata)(void *context, const char *name, const void *data, size_t size);
    void (*entry)(void *context, const char *path, uint32_t mode, const uint8_t *sha1);
} swipterm_package_callbacks;

swipterm_root_builder *swipterm_root_builder_create(const char *root_path, uint64_t disk_size, char **error_message);
bool swipterm_root_builder_add_file(swipterm_root_builder *builder, const char *archive_path, void *context,
                                    swipterm_progress_handler progress, char **error_message);
bool swipterm_root_builder_add_memory(swipterm_root_builder *builder, const void *data, size_t size, void *context,
                                      swipterm_progress_handler progress, char **error_message);
bool swipterm_root_builder_add_package(swipterm_root_builder *builder, const char *package_path,
                                       const swipterm_package_callbacks *callbacks, char **error_message);
int swipterm_root_builder_read(swipterm_root_builder *builder, const char *path, void **data_out, size_t *size_out);
int swipterm_root_builder_write(swipterm_root_builder *builder, const char *path, const void *data, size_t size,
                                uint32_t mode);
bool swipterm_root_builder_finish(swipterm_root_builder *builder, char **error_message);
void swipterm_root_builder_free(swipterm_root_builder *builder);

bool swipterm_root_export(const char *root_path, const char *archive_path,
                          void *context, swipterm_progress_handler progress, char **error_message);

swipterm_archive *swipterm_archive_open(const char *path, char **error_message);
int swipterm_archive_next(swipterm_archive *archive, const char **name_out, char **error_message);
ssize_t swipterm_archive_read(swipterm_archive *archive, void *buffer, size_t length, char **error_message);
void swipterm_archive_close(swipterm_archive *archive);

typedef enum {
    SWIPTERM_HALT_NONE = 0,
    SWIPTERM_HALT_EXIT = 1,
    SWIPTERM_HALT_REBOOT = 2,
    SWIPTERM_HALT_POWER_OFF = 3,
} swipterm_halt_reason;

typedef struct {
    const char *mime;
    const void *data;
    size_t length;
} swipterm_clipboard_item;

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
    int (*clipboard_read)(void *context, const char *mime, void **data_out, size_t *length_out);
    void (*clipboard_write)(void *context, size_t count, const swipterm_clipboard_item *items);
} swipterm_callbacks;

typedef enum {
    SWIPTERM_KERNEL_INSTALLED = 0,
    SWIPTERM_KERNEL_RELEASE = 1,
} swipterm_kernel_selection;

typedef enum {
    SWIPTERM_GPU_BACKEND_AUTOMATIC = 0,
    SWIPTERM_GPU_BACKEND_MOLTENVK = 1,
    SWIPTERM_GPU_BACKEND_KOSMICKRISP = 2,
} swipterm_gpu_backend;

typedef struct {
    bool enabled;
    swipterm_gpu_backend backend;
} swipterm_gpu_options;

typedef struct {
    const char *root_path;
    const char *hostname;
    const char *host_commands_directory;
    size_t ram_size;
    uint32_t cpu_count;
    swipterm_callbacks callbacks;
    const char *initramfs_path;
    swipterm_kernel_selection kernel;
    const char *kernel_release;
    const char *extra_cmdline;
    swipterm_network *network;
    swipterm_gpu_options gpu;
} swipterm_options;

typedef struct {
    char release[65];
    char fallback_reason[2048];
} swipterm_boot_info;

bool swipterm_jit_is_prepared(void);
bool swipterm_jit_is_enabled(void);
void swipterm_jit_set_enabled(bool enabled);
bool swipterm_jit_prepare(char **error_message);

uint32_t swipterm_gpu_available_backends(void);
bool swipterm_gpu_suspend(void);
void swipterm_gpu_resume(void);

int swipterm_boot(const swipterm_options *options, swipterm_runtime **runtime_out, char **error_message);
void swipterm_begin_shutdown(swipterm_runtime *runtime);
void swipterm_shutdown(swipterm_runtime *runtime);
int swipterm_start_init(swipterm_runtime *runtime, const char *path,
                        size_t argc, const char *const *argv, const char *const *envp);
void swipterm_suspend(swipterm_runtime *runtime);
void swipterm_resume(swipterm_runtime *runtime);
void swipterm_get_boot_info(swipterm_runtime *runtime, swipterm_boot_info *info_out);
bool swipterm_gpu_is_active(swipterm_runtime *runtime);

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
int swipterm_set_clipboard_offers(swipterm_runtime *runtime, size_t count, const char *const *mimes);

#endif
