#include "swiptermd.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <unistd.h>

enum {
    PROBE_INTERVAL_MS = 250,
    PROBE_LIMIT = 15 * 60 * 1000 / PROBE_INTERVAL_MS,
    PROBE_CONFIRMATIONS = 2,
    STATE_FILE_MAX = 64 * 1024,
    UTMP_RECORD_SIZE = 400,
    UTMP_TYPE_OFFSET = 0,
    UTMP_PID_OFFSET = 4,
    UTMP_ID_OFFSET = 40,
    UTMP_ID_SIZE = 4,
    UTMP_RUN_LVL = 1,
    UTMP_DEAD_PROCESS = 8,
};

static ev_node_t g_probe_ev;
static int g_probe_fd = -1;
static unsigned g_probe_count;
static unsigned g_probe_hits;

static char *read_root_file(const char *rel, size_t *len_out) {
    int fd = fs_open_in_root(rel, O_RDONLY | O_NOFOLLOW, 0);
    if (fd < 0)
        return NULL;
    struct stat st;
    char *data = NULL;
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size <= STATE_FILE_MAX &&
        (data = malloc((size_t)st.st_size + 1))) {
        if (read_all_blocking(fd, data, (size_t)st.st_size) == 0) {
            data[st.st_size] = '\0';
            *len_out = (size_t)st.st_size;
        } else {
            free(data);
            data = NULL;
        }
    }
    close(fd);
    return data;
}

static bool exists_in_root(const char *rel) {
    int fd = fs_open_in_root(rel, O_PATH | O_NOFOLLOW, 0);
    if (fd < 0)
        return false;
    close(fd);
    return true;
}

static bool openrc_settled(void) {
    size_t len = 0;
    char *level = read_root_file("run/openrc/softlevel", &len);
    if (!level)
        return false;
    level[strcspn(level, "\n")] = '\0';
    static const char *const transitional[] = {"", "sysinit", "boot", "single", "shutdown", "reboot"};
    bool settled = true;
    for (size_t i = 0; i < sizeof(transitional) / sizeof(transitional[0]); i++) {
        if (strcmp(level, transitional[i]) == 0)
            settled = false;
    }
    free(level);
    return settled && !exists_in_root("run/openrc/rc.starting") && !exists_in_root("run/openrc/rc.stopping");
}

static bool runit_stage2(void) {
    DIR *proc = opendir("/proc");
    if (!proc)
        return false;
    bool found = false;
    for (struct dirent *entry; !found && (entry = readdir(proc));) {
        char *end = NULL;
        long pid = strtol(entry->d_name, &end, 10);
        if (pid <= 0 || *end != '\0')
            continue;
        char path[64];
        char stat_line[512];
        snprintf(path, sizeof(path), "/proc/%ld/stat", pid);
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0)
            continue;
        ssize_t n = read(fd, stat_line, sizeof(stat_line) - 1);
        close(fd);
        if (n <= 0)
            continue;
        stat_line[n] = '\0';
        const char *fields = strrchr(stat_line, ')');
        int parent = 0;
        if (!fields || sscanf(fields + 1, " %*c %d", &parent) != 1 || parent != g_daemon.container_init_pid)
            continue;
        char exe[PATH_MAX];
        snprintf(path, sizeof(path), "/proc/%ld/exe", pid);
        ssize_t exe_len = readlink(path, exe, sizeof(exe) - 1);
        if (exe_len <= 0)
            continue;
        exe[exe_len] = '\0';
        const char *base = strrchr(exe, '/');
        found = base && strcmp(base + 1, "runsvdir") == 0;
    }
    closedir(proc);
    return found;
}

static const uint8_t *utmp_find_id(const uint8_t *utmp, size_t len, const char *id) {
    char key[UTMP_ID_SIZE] = {0};
    memcpy(key, id, strnlen(id, UTMP_ID_SIZE));
    for (size_t off = 0; off + UTMP_RECORD_SIZE <= len; off += UTMP_RECORD_SIZE) {
        const uint8_t *record = utmp + off;
        int16_t type;
        memcpy(&type, record + UTMP_TYPE_OFFSET, sizeof(type));
        if (type != UTMP_RUN_LVL && memcmp(record + UTMP_ID_OFFSET, key, UTMP_ID_SIZE) == 0)
            return record;
    }
    return NULL;
}

static char utmp_runlevel(const uint8_t *utmp, size_t len) {
    for (size_t off = 0; off + UTMP_RECORD_SIZE <= len; off += UTMP_RECORD_SIZE) {
        int16_t type;
        int32_t value;
        memcpy(&type, utmp + off + UTMP_TYPE_OFFSET, sizeof(type));
        memcpy(&value, utmp + off + UTMP_PID_OFFSET, sizeof(value));
        if (type == UTMP_RUN_LVL)
            return (char)(value & 0xff);
    }
    return '\0';
}

static bool sysvinit_settled(void) {
    size_t utmp_len = 0;
    uint8_t *utmp = (uint8_t *)read_root_file("var/run/utmp", &utmp_len);
    if (!utmp)
        return false;
    char level = utmp_runlevel(utmp, utmp_len);
    size_t inittab_len = 0;
    char *inittab = level >= '1' && level <= '5' ? read_root_file("etc/inittab", &inittab_len) : NULL;
    bool settled = inittab != NULL;
    char *save = NULL;
    for (char *line = inittab ? strtok_r(inittab, "\n", &save) : NULL; settled && line;
         line = strtok_r(NULL, "\n", &save)) {
        if (line[0] == '#')
            continue;
        char *fields[3];
        char *cursor = line;
        size_t count = 0;
        for (; count < 3 && cursor; count++)
            fields[count] = strsep(&cursor, ":");
        if (count < 3 || !cursor || strcmp(fields[2], "wait") != 0 || !strchr(fields[1], level))
            continue;
        const uint8_t *record = utmp_find_id(utmp, utmp_len, fields[0]);
        int16_t type = 0;
        if (record)
            memcpy(&type, record + UTMP_TYPE_OFFSET, sizeof(type));
        settled = type == UTMP_DEAD_PROCESS;
    }
    free(inittab);
    free(utmp);
    return settled;
}

static bool system_settled(void) {
    char init[NAME_MAX + 1];
    if (!ns_init_executable(init, sizeof(init)) || strcmp(init, "systemd") == 0)
        return false;
    return openrc_settled() || (strcmp(init, "runit") == 0 && runit_stage2()) || sysvinit_settled();
}

static void on_probe(ev_node_t *node, uint32_t events) {
    (void)events;
    uint64_t expirations;
    (void)read(node->fd, &expirations, sizeof(expirations));
    if ((g_daemon.ready_mask & (1U << SWIPTERMD_READY_SYSTEM)) || g_daemon.stopping ||
        g_daemon.container_init_pid <= 0 || ++g_probe_count > PROBE_LIMIT) {
        readiness_stop();
        return;
    }
    g_probe_hits = system_settled() ? g_probe_hits + 1 : 0;
    if (g_probe_hits >= PROBE_CONFIRMATIONS) {
        if (login_available())
            ready_announce(SWIPTERMD_READY_LOGIN);
        ready_announce(SWIPTERMD_READY_SYSTEM);
        readiness_stop();
    }
}

int readiness_start(void) {
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0)
        return -errno;
    struct itimerspec interval = {
        .it_value = {.tv_nsec = PROBE_INTERVAL_MS * 1000000L},
        .it_interval = {.tv_nsec = PROBE_INTERVAL_MS * 1000000L},
    };
    int rc = timerfd_settime(fd, 0, &interval, NULL) < 0 ? -errno : ev_add(&g_probe_ev, fd, EPOLLIN, on_probe, NULL);
    if (rc < 0) {
        close(fd);
        return rc;
    }
    g_probe_fd = fd;
    g_probe_count = 0;
    g_probe_hits = 0;
    return 0;
}

void readiness_stop(void) {
    if (g_probe_fd < 0)
        return;
    ev_del(&g_probe_ev);
    close(g_probe_fd);
    g_probe_fd = -1;
}
