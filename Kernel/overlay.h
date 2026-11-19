#ifndef SWIPTERM_OVERLAY_H
#define SWIPTERM_OVERLAY_H

#include <stddef.h>

struct swipterm_overlay_file {
    const char *path;
    const unsigned char *data;
    size_t length;
};

extern const int swipterm_overlay_version;
extern const struct swipterm_overlay_file swipterm_overlay_files[];
extern const size_t swipterm_overlay_file_count;

#endif
