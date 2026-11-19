#include <stdlib.h>

#include "tools/fakefs.h"

#include "swipterm.h"

struct progress_context {
    void *context;
    swipterm_progress_handler handler;
};

static void report_progress(void *cookie, double progress, const char *message, bool *cancel) {
    struct progress_context *context = cookie;
    if (context->handler != NULL)
        context->handler(context->context, progress);
}

static bool finish(bool succeeded, struct fakefsify_error *error, char **error_message) {
    if (!succeeded)
        *error_message = error->message;
    return succeeded;
}

bool swipterm_image_import_file(const char *archive_path, const char *root_path,
                                void *context, swipterm_progress_handler progress, char **error_message) {
    struct progress_context progress_context = {context, progress};
    struct fakefsify_error error = {};
    bool succeeded = fakefs_import(archive_path, root_path, &error,
                                   (struct progress) {&progress_context, report_progress});
    return finish(succeeded, &error, error_message);
}

bool swipterm_image_import_memory(const void *data, size_t size, const char *root_path,
                                  void *context, swipterm_progress_handler progress, char **error_message) {
    struct progress_context progress_context = {context, progress};
    struct fakefsify_error error = {};
    bool succeeded = fakefs_import_memory(data, size, root_path, &error,
                                          (struct progress) {&progress_context, report_progress});
    return finish(succeeded, &error, error_message);
}

bool swipterm_image_export(const char *root_path, const char *archive_path,
                           void *context, swipterm_progress_handler progress, char **error_message) {
    struct progress_context progress_context = {context, progress};
    struct fakefsify_error error = {};
    bool succeeded = fakefs_export(root_path, archive_path, &error,
                                   (struct progress) {&progress_context, report_progress});
    return finish(succeeded, &error, error_message);
}
