/**************************************************************************
 *
 * Copyright (C) 2019 Chromium.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
    **************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "virgl_util.h"

#include <errno.h>
#ifdef HAVE_EVENTFD_H
#include <sys/eventfd.h>
#endif
#ifdef HAVE_EVENTFD_EMULATION
#include <fcntl.h>
#ifndef F_SETNOSIGPIPE
#include <sys/socket.h>
#endif
#endif
#include <unistd.h>

#include "util/os_misc.h"
#include "util/u_pointer.h"
#include "util/u_string.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef HAVE_DMABUF_H
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if ENABLE_TRACING == TRACE_WITH_PERFETTO
#include <vperfetto-min.h>
#endif

#if ENABLE_TRACING == TRACE_WITH_SYSPROF
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <sysprof-capture.h>
#pragma GCC diagnostic pop
#endif

#if ENABLE_TRACING == TRACE_WITH_STDERR
#include <stdio.h>
#endif

uint32_t hash_func_u32(const void *key)
{
   intptr_t ip = pointer_to_intptr(key);
   return (uint32_t)(ip & 0xffffffff);
}

struct hash_entry *
hash_table_search(struct hash_table *ht, uint32_t key)
{
   /* zero is not a valid key for u32_keys hashtable: */
   if (!key)
      return NULL;
   return _mesa_hash_table_search(ht, (void *)(uintptr_t)key);
}

bool equal_func(const void *key1, const void *key2)
{
   return key1 == key2;
}

bool has_eventfd(void)
{
#if defined(HAVE_EVENTFD_H) || defined(HAVE_EVENTFD_EMULATION)
   return true;
#else
   return false;
#endif
}

#ifdef HAVE_EVENTFD_H

int create_eventfd(unsigned int initval, struct virgl_eventfd *out_eventfd)
{
   const int fd = eventfd(initval, EFD_CLOEXEC | EFD_NONBLOCK);
   if (fd < 0)
      return -1;

   /* a real eventfd is both readable and writable */
   out_eventfd->read_fd = fd;
   out_eventfd->write_fd = fd;

   return 0;
}

#elif defined(HAVE_EVENTFD_EMULATION)

static int set_fd_flags(int fd)
{
   int flags = fcntl(fd, F_GETFL);
   if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
      return -1;

   flags = fcntl(fd, F_GETFD);
   if (flags < 0 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0)
      return -1;

   return 0;
}

int create_eventfd(unsigned int initval, struct virgl_eventfd *out_eventfd)
{
   int fds[2];
   bool needs_fd_flags = false;

   out_eventfd->read_fd = -1;
   out_eventfd->write_fd = -1;

#ifdef F_SETNOSIGPIPE
#ifdef HAVE_PIPE2
   const int ret = pipe2(fds, O_CLOEXEC | O_NONBLOCK);
#else
   const int ret = pipe(fds);
   needs_fd_flags = true;
#endif
   if (ret)
      return -1;

   /* A render worker can signal after the proxy closes the read end.
    * This flag follows the write end when sent to the worker via SCM_RIGHTS.
    */
   if (fcntl(fds[1], F_SETNOSIGPIPE, 1) < 0)
      goto fail;
#else
   /* Use a socket so writes can suppress SIGPIPE with MSG_NOSIGNAL. */
   int type = SOCK_DGRAM;
#ifdef SOCK_CLOEXEC
   type |= SOCK_CLOEXEC;
#else
   needs_fd_flags = true;
#endif
#ifdef SOCK_NONBLOCK
   type |= SOCK_NONBLOCK;
#else
   needs_fd_flags = true;
#endif
   if (socketpair(AF_UNIX, type, 0, fds))
      return -1;
#endif

   if (needs_fd_flags && (set_fd_flags(fds[0]) || set_fd_flags(fds[1])))
      goto fail;

   out_eventfd->read_fd = fds[0];
   out_eventfd->write_fd = fds[1];

   /* eventfd(2) starts out readable when initval is non-zero */
   if (initval && write_eventfd(out_eventfd->write_fd, initval)) {
      close_eventfd(out_eventfd);
      return -1;
   }

   return 0;

fail:
   close(fds[0]);
   close(fds[1]);
   return -1;
}

#else

int create_eventfd(UNUSED unsigned int initval, struct virgl_eventfd *out_eventfd)
{
   out_eventfd->read_fd = -1;
   out_eventfd->write_fd = -1;
   return -1;
}

#endif

void close_eventfd(struct virgl_eventfd *eventfd)
{
   if (eventfd->read_fd >= 0)
      close(eventfd->read_fd);

   /* with a real eventfd, read_fd and write_fd are the same fd */
   if (eventfd->write_fd >= 0 && eventfd->write_fd != eventfd->read_fd)
      close(eventfd->write_fd);

   eventfd->read_fd = -1;
   eventfd->write_fd = -1;
}

int write_eventfd(int fd, uint64_t val)
{
   const char *buf = (const char *)&val;
   size_t count = sizeof(val);
   ssize_t ret = 0;

   while (count) {
#if defined(HAVE_EVENTFD_EMULATION) && !defined(F_SETNOSIGPIPE)
      ret = send(fd, buf, count, MSG_NOSIGNAL);
      /* vtest sync waits can also pass a FIFO to this helper. */
      if (ret < 0 && errno == ENOTSOCK)
         ret = write(fd, buf, count);
#else
      ret = write(fd, buf, count);
#endif
      if (ret < 0) {
         if (errno == EINTR)
            continue;
         break;
      }
      count -= ret;
      buf += ret;
   }

   return count ? -1 : 0;
}

void flush_eventfd(int fd)
{
    ssize_t len;
    uint64_t value;
    do {
       len = read(fd, &value, sizeof(value));
    } while ((len == -1 && errno == EINTR) || len == sizeof(value));
}

const struct log_levels_lut {
   char *name;
   enum virgl_log_level_flags log_level;
} log_levels_table[] = {
   {"debug", VIRGL_LOG_LEVEL_DEBUG},
   {"info", VIRGL_LOG_LEVEL_INFO},
   {"warning", VIRGL_LOG_LEVEL_WARNING},
   {"error", VIRGL_LOG_LEVEL_ERROR},
   {"silent", VIRGL_LOG_LEVEL_SILENT},
   { NULL, 0 },
};

#ifndef NDEBUG
static enum virgl_log_level_flags virgl_log_level = VIRGL_LOG_LEVEL_WARNING;
#else
static enum virgl_log_level_flags virgl_log_level = VIRGL_LOG_LEVEL_ERROR;
#endif
static bool virgl_log_level_initialized = false;

static FILE *
virgl_default_log_file(void)
{
   static FILE* fp = NULL;
   if (NULL == fp) {
      const char* log = getenv("VIRGL_LOG_FILE");
      if (log) {
         char *log_prefix = strdup(log);
         char *log_suffix = strstr(log_prefix, "%PID%");
         if (log_suffix) {
            *log_suffix = 0;
            log_suffix += 5;
            int len = strlen(log) + 32;
            char *name = malloc(len);
            snprintf(name, len, "%s%d%s", log_prefix, getpid(), log_suffix);
            fp = fopen(name, "a");
            free(name);
         } else {
            fp = fopen(log, "a");
         }
         free(log_prefix);
         if (NULL == fp) {
            fprintf(stderr, "Can't open %s\n", log);
            fp = stderr;
         }
      } else {
            fp = stderr;
      }
   }

   return fp;
}

static bool
virgl_default_log_enabled(enum virgl_log_level_flags log_level)
{
   if (!virgl_log_level_initialized) {
      const char* log_level_env = getenv("VIRGL_LOG_LEVEL");
      if (log_level_env != NULL && log_level_env[0] != '\0') {
         int log_index = 0;
         const struct log_levels_lut *lut = &log_levels_table[0];
         while (lut->name) {
            if (!strcmp(lut->name, log_level_env)) {
               virgl_log_level = lut->log_level;
               break;
            }

            lut = &log_levels_table[++log_index];
         }

         if (!lut->name)
            fprintf(virgl_default_log_file(), "Unknown log level %s requested\n",
                    log_level_env);
      }

      virgl_log_level_initialized = true;
   }

   return log_level >= virgl_log_level;
}

static
void virgl_default_logger(enum virgl_log_level_flags log_level,
                          const char *message,
                          UNUSED void* user_data)
{
   if (!virgl_default_log_enabled(log_level))
      return;

   FILE *fp = virgl_default_log_file();
   fprintf(fp, "%s", message);
   fflush(fp);
}

void virgl_override_log_level(enum virgl_log_level_flags log_level)
{
   virgl_log_level = log_level;
   virgl_log_level_initialized = true;
}

static struct {
   virgl_log_callback_type log_cb;
   virgl_free_data_callback_type free_data_cb;
   void *user_data;
} virgl_log_data = { virgl_default_logger, NULL, NULL };

static bool
virgl_log_level_enabled(enum virgl_log_level_flags log_level)
{
   if (!virgl_log_data.log_cb)
      return false;

   /* Only filter the default logger; custom callbacks receive all levels. */
   return virgl_log_data.log_cb != virgl_default_logger ||
          virgl_default_log_enabled(log_level);
}

void virgl_log_set_handler(virgl_log_callback_type log_cb,
                           void *user_data,
                           virgl_free_data_callback_type free_data_cb)
{
   if (virgl_log_data.free_data_cb)
      virgl_log_data.free_data_cb(virgl_log_data.user_data);

   virgl_log_data.log_cb = log_cb;
   virgl_log_data.free_data_cb = free_data_cb;
   virgl_log_data.user_data = user_data;
}

void virgl_logv(enum virgl_log_level_flags log_level, const char *fmt, va_list va)
{
   char *str = NULL;

   if (!virgl_log_level_enabled(log_level))
      return;

   if (vasprintf(&str, fmt, va) < 0)
      return;

   virgl_log_data.log_cb(log_level, str, virgl_log_data.user_data);
   free (str);
}

void virgl_prefixed_logv(const char *domain,
                         enum virgl_log_level_flags log_level,
                         const char *fmt,
                         va_list va)
{
   char *prefixed_fmt = NULL;
   char line_end = '\0';

   if (!virgl_log_level_enabled(log_level))
      return;

   assert(strchr(domain,'%') == NULL);

   /* add new line if needed */
   if (fmt[strlen(fmt)-1] != '\n')
      line_end = '\n';

   if (asprintf(&prefixed_fmt, "%s: %s%c", domain, fmt, line_end) < 0)
      return;

   virgl_logv(log_level, prefixed_fmt, va);
   free (prefixed_fmt);
}

#if ENABLE_TRACING == TRACE_WITH_PERCETTO
PERCETTO_CATEGORY_DEFINE(VIRGL_PERCETTO_CATEGORIES)

void trace_init(void)
{
  PERCETTO_INIT(PERCETTO_CLOCK_DONT_CARE);
}
#endif

#if ENABLE_TRACING == TRACE_WITH_PERFETTO
static void on_tracing_state_change(bool enabled) {
    virgl_debug("%s: tracing state change: %d\n", __func__, enabled);
}

void trace_init(void)
{
   struct vperfetto_min_config config = {
      .on_tracing_state_change = on_tracing_state_change,
      .init_flags = VPERFETTO_INIT_FLAG_USE_SYSTEM_BACKEND,
            .filename = NULL,
            .shmem_size_hint_kb = 32 * 1024,
   };

   vperfetto_min_startTracing(&config);
}

void *trace_begin(const char *scope)
{
   vperfetto_min_beginTrackEvent_VMM(scope);
   return NULL;
}

void trace_end(void **dummy)
{
   (void)dummy;
   vperfetto_min_endTrackEvent_VMM();
}
#endif

#if ENABLE_TRACING == TRACE_WITH_SYSPROF
struct virgl_sysprof_entry {
   SysprofTimeStamp begin;
   /* SysprofCaptureMark itself limits it to 40 characters */
   char name[40];
};

void trace_init(void)
{
}

void *trace_begin(const char *scope)
{
   struct virgl_sysprof_entry *trace = malloc(sizeof (struct virgl_sysprof_entry));
   trace->begin = SYSPROF_CAPTURE_CURRENT_TIME;
   snprintf(trace->name, sizeof(trace->name), "%s", scope);

   return trace;
}

void trace_end(void **func_name)
{
   struct virgl_sysprof_entry *trace = (struct virgl_sysprof_entry *)*func_name;
   sysprof_collector_mark(trace->begin,
                          SYSPROF_CAPTURE_CURRENT_TIME - trace->begin,
                          "virglrenderer",
                          trace->name,
                          NULL);
   free(trace);
}
#endif

#if ENABLE_TRACING == TRACE_WITH_STDERR
static int nesting_depth = 0;
void trace_init(void)
{
}

void *trace_begin(const char *scope)
{
   for (int i = 0; i < nesting_depth; ++i)
      fprintf(stderr, "  ");

   fprintf(stderr, "ENTER:%s\n", scope);
   nesting_depth++;

   return (void *)scope;
}

void trace_end(void **func_name)
{
   --nesting_depth;
   for (int i = 0; i < nesting_depth; ++i)
      fprintf(stderr, "  ");
   fprintf(stderr, "LEAVE %s\n", (const char *) *func_name);
}
#endif

void set_dmabuf_name(int fd, const char *name)
{
   #ifdef DMA_BUF_SET_NAME_B
   if (name && *name != '\0')
      ioctl(fd, DMA_BUF_SET_NAME_B, name);
   #else
   (void)fd;
   (void)name;
   #endif
}
