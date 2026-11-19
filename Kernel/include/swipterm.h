#ifndef SWIPTERM_H
#define SWIPTERM_H

#include <stdbool.h>
#include <stddef.h>

typedef void (*swipterm_progress_handler)(void *context, double fraction);
typedef void (*swipterm_output_handler)(void *context, const void *data, size_t length);

bool swipterm_root_import(const char *archive_path, const char *root_path,
                          void *context, swipterm_progress_handler progress,
                          char **error_message);

typedef struct {
    const char *root_path;
    const char *socket_prefix;
    const char *version;
    const char *const *init_argv;
    const char *const *init_envp;
    void *console_context;
    swipterm_output_handler console_output;
} swipterm_options;

int swipterm_boot(const swipterm_options *options);
int swipterm_update_resolv_conf(void);
int swipterm_read_file(const char *path, void **data_out, size_t *length_out);
int swipterm_write_file(const char *path, const void *data, size_t length);

typedef struct swipterm_terminal swipterm_terminal;

typedef struct {
    void *context;
    swipterm_output_handler output;
    // Runs with the kernel's process table locked: must not block or call back into the kernel.
    void (*exit)(void *context, int status);
} swipterm_terminal_handlers;

swipterm_terminal *swipterm_terminal_open(const char *const *argv, const char *const *envp,
                                          unsigned short columns, unsigned short rows,
                                          swipterm_terminal_handlers handlers, int *error_out);
void swipterm_terminal_send_input(swipterm_terminal *terminal, const void *data, size_t length);
void swipterm_terminal_resize(swipterm_terminal *terminal, unsigned short columns, unsigned short rows);
void swipterm_terminal_set_output_suspended(swipterm_terminal *terminal, bool suspended);
void swipterm_terminal_close(swipterm_terminal *terminal);

#endif
