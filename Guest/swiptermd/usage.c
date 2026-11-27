#include "swiptermd.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/acct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#define ACCT_DIR "/run/acct"
#define MAX_USAGE_REPORT_ITEMS 512U

typedef struct tracked_proc {
    pid_t pid;
    pid_t ppid;
    uint64_t starttime;
    uint64_t self_ticks;
    uint64_t child_ticks;
    char name[64];
    char state;
    bool matched_alive;
    bool matched_exit;
    bool acct_done;
} tracked_proc_t;

typedef struct usage_item {
    char name[64];
    uint64_t ticks;
} usage_item_t;

typedef struct usage_accum {
    usage_item_t *items;
    size_t len;
    size_t cap;
} usage_accum_t;

static tracked_proc_t *g_tracked;
static size_t g_tracked_len;
static bool g_acct_enabled;
static int g_acct_slot;

static bool create_empty_acct_file(const char *path) {
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0600);
    if (fd < 0)
        return false;
    close(fd);
    return true;
}

void usage_setup(void) {
    if (mkdir(ACCT_DIR, 0700) < 0 && errno != EEXIST)
        return;
    if (mount("tmpfs", ACCT_DIR, "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, "mode=0700,size=16m") < 0)
        return;
    static const char initial_path[] = ACCT_DIR "/0";
    if (!create_empty_acct_file(initial_path))
        return;
    if (acct(initial_path) == 0) {
        g_acct_enabled = true;
        g_acct_slot = 0;
    } else {
        unlink(initial_path);
    }
}

void usage_shutdown(void) {
    if (g_acct_enabled) {
        (void)acct(NULL);
        g_acct_enabled = false;
    }
    free(g_tracked);
    g_tracked = NULL;
    g_tracked_len = 0;
}

static uint64_t decode_comp_t(uint16_t v) {
    return (uint64_t)(v & 0x1fffU) << (((v >> 13) & 0x7U) * 3U);
}

static void normalize_name(const char *raw, size_t raw_len, char out[64]) {
    while (raw_len > 0 && raw[0] == '-') {
        raw++;
        raw_len--;
    }
    if (raw_len >= 64)
        raw_len = 63;
    if (raw_len > 0)
        memcpy(out, raw, raw_len);
    out[raw_len] = '\0';
}

static void resolve_live_name(pid_t pid, const char *comm, size_t comm_len, char out[64]) {
    if (comm_len >= 15) {
        const char *comm_trimmed = comm[0] == '-' ? comm + 1 : comm;
        size_t prefix = comm_len - (size_t)(comm_trimmed - comm);
        if (prefix > 0) {
            char path[64];
            char exe[PATH_MAX];
            snprintf(path, sizeof(path), "/proc/%d/exe", (int)pid);
            ssize_t exe_len = readlink(path, exe, sizeof(exe) - 1);
            if (exe_len > 0) {
                exe[exe_len] = '\0';
                static const char deleted[] = " (deleted)";
                if ((size_t)exe_len > sizeof(deleted) - 1 &&
                    strcmp(exe + (size_t)exe_len - (sizeof(deleted) - 1), deleted) == 0) {
                    exe[(size_t)exe_len - (sizeof(deleted) - 1)] = '\0';
                }
                const char *slash = strrchr(exe, '/');
                const char *base = slash ? slash + 1 : exe;
                if (strncmp(base, comm_trimmed, prefix) == 0) {
                    normalize_name(base, strlen(base), out);
                    if (out[0] != '\0')
                        return;
                }
            }
            snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
            int fd = open(path, O_RDONLY | O_CLOEXEC);
            if (fd >= 0) {
                char cmd[512];
                ssize_t n = read(fd, cmd, sizeof(cmd) - 1);
                close(fd);
                if (n > 0) {
                    cmd[n] = '\0';
                    cmd[strcspn(cmd, " \t:\r\n")] = '\0';
                    const char *slash = strrchr(cmd, '/');
                    const char *base = slash ? slash + 1 : cmd;
                    const char *trimmed = base[0] == '-' ? base + 1 : base;
                    if (strncmp(trimmed, comm_trimmed, prefix) == 0) {
                        normalize_name(trimmed, strlen(trimmed), out);
                        if (out[0] != '\0')
                            return;
                    }
                }
            }
        }
    }
    normalize_name(comm, comm_len, out);
}

static bool accum_add(usage_accum_t *acc, const char *name, uint64_t ticks) {
    if (ticks == 0 || name[0] == '\0')
        return true;
    for (size_t i = 0; i < acc->len; i++) {
        if (strcmp(acc->items[i].name, name) == 0) {
            acc->items[i].ticks += ticks;
            return true;
        }
    }
    if (acc->len == acc->cap) {
        size_t new_cap = acc->cap ? acc->cap * 2 : 32;
        usage_item_t *nb = realloc(acc->items, new_cap * sizeof(*nb));
        if (!nb)
            return false;
        acc->items = nb;
        acc->cap = new_cap;
    }
    usage_item_t *it = &acc->items[acc->len++];
    snprintf(it->name, sizeof(it->name), "%s", name);
    it->ticks = ticks;
    return true;
}

static bool is_workload_cgroup(pid_t pid) {
    char path[64];
    char group[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/cgroup", (int)pid);
    if (!cgroup_membership(path, group, sizeof(group)))
        return false;
    return strcmp(group, "/machine") == 0 || strncmp(group, "/machine/", 9) == 0 ||
           strcmp(group, "/disks") == 0 || strncmp(group, "/disks/", 7) == 0;
}

static bool is_dead_state(char state) {
    return state == 'Z' || state == 'X' || state == 'x';
}

static bool parse_proc_stat(pid_t pid, char *state_out, pid_t *ppid_out, uint64_t *self_out,
                            uint64_t *child_out, uint64_t *start_out, char name_out[64]) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    char buf[2048];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return false;
    buf[n] = '\0';

    char *lparen = strchr(buf, '(');
    char *rparen = strrchr(buf, ')');
    if (!lparen || !rparen || rparen <= lparen || rparen[1] != ' ')
        return false;

    resolve_live_name(pid, lparen + 1, (size_t)(rparen - lparen - 1), name_out);
    if (name_out[0] == '\0')
        return false;

    char *save = NULL;
    char *tok = strtok_r(rparen + 2, " \t\n", &save);
    char state = '\0';
    long ppid = 0;
    unsigned long long utime = 0, stime = 0, cutime = 0, cstime = 0, starttime = 0;
    int idx = 0;
    while (tok && idx <= 19) {
        switch (idx) {
        case 0:
            state = tok[0];
            break;
        case 1:
            ppid = strtol(tok, NULL, 10);
            break;
        case 11:
            utime = strtoull(tok, NULL, 10);
            break;
        case 12:
            stime = strtoull(tok, NULL, 10);
            break;
        case 13:
            cutime = strtoull(tok, NULL, 10);
            break;
        case 14:
            cstime = strtoull(tok, NULL, 10);
            break;
        case 19:
            starttime = strtoull(tok, NULL, 10);
            break;
        default:
            break;
        }
        idx++;
        tok = strtok_r(NULL, " \t\n", &save);
    }
    if (idx <= 19)
        return false;

    *state_out = state;
    *ppid_out = (pid_t)ppid;
    *self_out = (uint64_t)utime + (uint64_t)stime;
    *child_out = (uint64_t)cutime + (uint64_t)cstime;
    *start_out = (uint64_t)starttime;
    return true;
}

static int rotate_and_open_acct(char old_path[32]) {
    if (!g_acct_enabled)
        return -1;
    int old_slot = g_acct_slot;
    int new_slot = 1 - old_slot;
    char new_path[32];
    snprintf(old_path, 32, ACCT_DIR "/%d", old_slot);
    snprintf(new_path, sizeof(new_path), ACCT_DIR "/%d", new_slot);
    if (!create_empty_acct_file(new_path))
        return -1;
    if (acct(new_path) < 0) {
        unlink(new_path);
        return -1;
    }
    g_acct_slot = new_slot;
    return open(old_path, O_RDONLY | O_CLOEXEC);
}

static int compare_items_desc(const void *a, const void *b) {
    const usage_item_t *ia = a;
    const usage_item_t *ib = b;
    if (ia->ticks != ib->ticks)
        return ia->ticks > ib->ticks ? -1 : 1;
    return strcmp(ia->name, ib->name);
}

void usage_handle_read(uint32_t req_id) {
    char old_acct_path[32] = {0};
    int acct_fd = rotate_and_open_acct(old_acct_path);
    bool use_reaped_fallback = acct_fd < 0;

    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        if (acct_fd >= 0) {
            close(acct_fd);
            unlink(old_acct_path);
        }
        ctrl_send_err_reply(SWIPTERMD_OP_READ_USAGE, req_id, -errno);
        return;
    }

    for (size_t i = 0; i < g_tracked_len; i++) {
        g_tracked[i].matched_alive = false;
        g_tracked[i].matched_exit = false;
    }

    tracked_proc_t *next = NULL;
    uint64_t *reaped_budget = NULL;
    size_t next_len = 0;
    size_t next_cap = 0;
    usage_accum_t acc = {0};
    bool oom = false;

    struct dirent *de;
    while ((de = readdir(proc_dir)) != NULL) {
        if (!isdigit((unsigned char)de->d_name[0]))
            continue;
        char *end = NULL;
        long parsed_pid = strtol(de->d_name, &end, 10);
        if (!end || *end != '\0' || parsed_pid <= 1)
            continue;
        pid_t pid = (pid_t)parsed_pid;
        if (!is_workload_cgroup(pid))
            continue;

        char state = '\0';
        pid_t ppid = 0;
        uint64_t self_ticks = 0, child_ticks = 0, starttime = 0;
        char name[64];
        if (!parse_proc_stat(pid, &state, &ppid, &self_ticks, &child_ticks, &starttime, name))
            continue;

        if (next_len == next_cap) {
            size_t nc = next_cap ? next_cap * 2 : 64;
            tracked_proc_t *nt = realloc(next, nc * sizeof(*nt));
            if (!nt) {
                oom = true;
                break;
            }
            next = nt;
            if (use_reaped_fallback) {
                uint64_t *nb = realloc(reaped_budget, nc * sizeof(*nb));
                if (!nb) {
                    oom = true;
                    break;
                }
                reaped_budget = nb;
            }
            next_cap = nc;
        }

        uint64_t delta_self = self_ticks;
        uint64_t delta_child = child_ticks;
        bool acct_done = false;
        for (size_t i = 0; i < g_tracked_len; i++) {
            if (g_tracked[i].pid == pid && g_tracked[i].starttime == starttime) {
                g_tracked[i].matched_alive = true;
                acct_done = g_tracked[i].acct_done;
                delta_self = self_ticks >= g_tracked[i].self_ticks ? self_ticks - g_tracked[i].self_ticks : 0;
                delta_child = child_ticks >= g_tracked[i].child_ticks ? child_ticks - g_tracked[i].child_ticks : 0;
                break;
            }
        }

        next[next_len] = (tracked_proc_t){
            .pid = pid,
            .ppid = ppid,
            .starttime = starttime,
            .self_ticks = self_ticks,
            .child_ticks = child_ticks,
            .state = state,
            .acct_done = acct_done,
        };
        memcpy(next[next_len].name, name, sizeof(name));
        if (use_reaped_fallback)
            reaped_budget[next_len] = delta_child;
        next_len++;

        if (!accum_add(&acc, name, delta_self)) {
            oom = true;
            break;
        }
    }
    closedir(proc_dir);

    if (!oom && acct_fd >= 0) {
        struct acct_v3 recs[64];
        for (;;) {
            ssize_t n = read(acct_fd, recs, sizeof(recs));
            if (n < 0 && errno == EINTR)
                continue;
            if (n < (ssize_t)sizeof(struct acct_v3))
                break;
            size_t count = (size_t)n / sizeof(struct acct_v3);
            for (size_t i = 0; i < count; i++) {
                const struct acct_v3 *rec = &recs[i];
                if ((rec->ac_version & 0x7f) != 3)
                    continue;
                bool in_container = (g_daemon.container_init_pid > 0 &&
                                     rec->ac_pid == (uint32_t)g_daemon.container_init_pid) ||
                                    (rec->ac_ppid > 2);
                if (!in_container)
                    continue;

                bool handled_by_live_zombie = false;
                for (size_t k = 0; k < next_len; k++) {
                    if (next[k].pid == (pid_t)rec->ac_pid && !next[k].acct_done && is_dead_state(next[k].state)) {
                        next[k].acct_done = true;
                        handled_by_live_zombie = true;
                        break;
                    }
                }
                if (handled_by_live_zombie)
                    continue;

                uint64_t total_ticks = decode_comp_t(rec->ac_utime) + decode_comp_t(rec->ac_stime);
                tracked_proc_t *matched = NULL;
                for (size_t j = 0; j < g_tracked_len; j++) {
                    if (!g_tracked[j].matched_alive && !g_tracked[j].matched_exit &&
                        g_tracked[j].pid == (pid_t)rec->ac_pid) {
                        matched = &g_tracked[j];
                        matched->matched_exit = true;
                        break;
                    }
                }

                if (matched) {
                    if (!matched->acct_done && !is_dead_state(matched->state)) {
                        uint64_t delta = total_ticks >= matched->self_ticks ? total_ticks - matched->self_ticks : 0;
                        if (!accum_add(&acc, matched->name, delta)) {
                            oom = true;
                            break;
                        }
                    }
                } else if (total_ticks > 0) {
                    size_t raw_len = strnlen(rec->ac_comm, ACCT_COMM);
                    char cname[64];
                    normalize_name(rec->ac_comm, raw_len, cname);
                    if (!accum_add(&acc, cname, total_ticks)) {
                        oom = true;
                        break;
                    }
                }
            }
            if (oom)
                break;
        }
    }
    if (acct_fd >= 0) {
        close(acct_fd);
        unlink(old_acct_path);
    }

    if (!oom && use_reaped_fallback) {
        for (size_t j = 0; j < g_tracked_len; j++) {
            if (g_tracked[j].matched_alive)
                continue;
            for (size_t p = 0; p < next_len; p++) {
                if (next[p].pid == g_tracked[j].ppid) {
                    uint64_t prev_self = g_tracked[j].self_ticks;
                    reaped_budget[p] = reaped_budget[p] > prev_self ? reaped_budget[p] - prev_self : 0;
                    break;
                }
            }
        }
        for (size_t p = 0; p < next_len; p++) {
            if (reaped_budget[p] > 0 && !accum_add(&acc, next[p].name, reaped_budget[p])) {
                oom = true;
                break;
            }
        }
    }

    free(reaped_budget);
    if (oom) {
        free(next);
        free(acc.items);
        ctrl_send_err_reply(SWIPTERMD_OP_READ_USAGE, req_id, -ENOMEM);
        return;
    }

    free(g_tracked);
    g_tracked = next;
    g_tracked_len = next_len;

    if (acc.len > 1)
        qsort(acc.items, acc.len, sizeof(*acc.items), compare_items_desc);

    long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0)
        clk_tck = 100;

    uint32_t out_count = 0;
    size_t wire_cap = 8;
    for (size_t i = 0; i < acc.len && out_count < MAX_USAGE_REPORT_ITEMS; i++) {
        uint64_t usec = (acc.items[i].ticks * 1000000ULL) / (uint64_t)clk_tck;
        if (usec == 0)
            continue;
        acc.items[out_count] = acc.items[i];
        acc.items[out_count].ticks = usec;
        wire_cap += 4 + strlen(acc.items[out_count].name) + 8;
        out_count++;
    }

    uint8_t *reply = malloc(wire_cap);
    if (!reply) {
        free(acc.items);
        ctrl_send_err_reply(SWIPTERMD_OP_READ_USAGE, req_id, -ENOMEM);
        return;
    }

    swiptermd_buf_w_t w = {.data = reply, .cap = wire_cap};
    swiptermd_w_u32(&w, 0);
    swiptermd_w_u32(&w, out_count);
    for (uint32_t i = 0; i < out_count; i++) {
        swiptermd_w_str(&w, acc.items[i].name);
        swiptermd_w_u64(&w, acc.items[i].ticks);
    }
    ctrl_send_frame(SWIPTERMD_OP_READ_USAGE | SWIPTERMD_OP_REPLY_FLAG, req_id, reply, (uint32_t)w.len);
    free(reply);
    free(acc.items);
}
