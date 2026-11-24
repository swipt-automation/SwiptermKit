#include "swiptermd.h"

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

enum {
    LOGIN_ENV_MAX = 64,
    LOGIN_GROUPS_MAX = 64,
};

static const char *const login_paths[] = {
    "/bin/login",
    "/usr/bin/login",
    "/run/current-system/sw/bin/login",
};

static bool login_env_allowed(const char *entry) {
    static const char *const names[] = {
        "TERM=", "COLORTERM=", "LANG=", "LANGUAGE=", "BROWSER=", "TZ=", "WAYLAND_DISPLAY=",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (strncmp(entry, names[i], strlen(names[i])) == 0)
            return true;
    }
    const char *eq = strchr(entry, '=');
    if (strncmp(entry, "LC_", 3) != 0 || !eq || eq == entry + 3)
        return false;
    for (const char *p = entry + 3; p < eq; p++) {
        if ((*p < 'A' || *p > 'Z') && *p != '_')
            return false;
    }
    return true;
}

static bool login_value_safe(const char *value) {
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p < 0x20 || *p == 0x7F)
            return false;
    }
    return true;
}

static size_t login_environment(char *const requested[], char **out, size_t cap) {
    size_t count = 0;
    for (size_t i = 0; requested && requested[i] && count < cap; i++) {
        if (login_env_allowed(requested[i]) && login_value_safe(strchr(requested[i], '=') + 1))
            out[count++] = requested[i];
    }
    return count;
}

typedef struct login_account {
    uid_t uid;
    gid_t gid;
    char home[PATH_MAX];
    char shell[PATH_MAX];
} login_account_t;

bool login_name_valid(const char *name) {
    size_t len = strlen(name);
    if (len == 0 || len >= LOGIN_USER_CAP)
        return false;
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        bool leading = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        bool trailing = (c >= '0' && c <= '9') || c == '.' || c == '-';
        bool machine = c == '$' && i > 0 && i == len - 1;
        if (!leading && !(i > 0 && trailing) && !machine)
            return false;
    }
    return true;
}

static int read_account(const char *user, login_account_t *account) {
    FILE *fp = fopen("/etc/passwd", "re");
    if (!fp)
        return -errno;
    int rc = -ENOENT;
    char line[2 * PATH_MAX + 512];
    while (fgets(line, sizeof(line), fp)) {
        char *fields[7];
        char *cursor = line;
        size_t n = 0;
        for (; n < 7 && cursor; n++)
            fields[n] = strsep(&cursor, ":");
        if (n < 7 || strcmp(fields[0], user) != 0)
            continue;
        fields[6][strcspn(fields[6], "\n")] = '\0';
        char *uid_end = NULL;
        char *gid_end = NULL;
        unsigned long uid = strtoul(fields[2], &uid_end, 10);
        unsigned long gid = strtoul(fields[3], &gid_end, 10);
        if (fields[2][0] == '\0' || *uid_end != '\0' || fields[3][0] == '\0' || *gid_end != '\0' ||
            uid > UINT32_MAX - 1 || gid > UINT32_MAX - 1) {
            rc = -EINVAL;
            break;
        }
        if (snprintf(account->home, sizeof(account->home), "%s", fields[5][0] == '/' ? fields[5] : "/") >=
                (int)sizeof(account->home) ||
            snprintf(account->shell, sizeof(account->shell), "%s", fields[6][0] == '/' ? fields[6] : "/bin/sh") >=
                (int)sizeof(account->shell)) {
            rc = -ENAMETOOLONG;
            break;
        }
        account->uid = (uid_t)uid;
        account->gid = (gid_t)gid;
        rc = 0;
        break;
    }
    fclose(fp);
    return rc;
}

static bool group_lists_member(char *members, const char *user) {
    char *save = NULL;
    for (char *member = strtok_r(members, ",\n", &save); member; member = strtok_r(NULL, ",\n", &save)) {
        if (strcmp(member, user) == 0)
            return true;
    }
    return false;
}

static size_t account_groups(const char *user, gid_t primary, gid_t *groups, size_t cap) {
    size_t count = 0;
    groups[count++] = primary;
    FILE *fp = fopen("/etc/group", "re");
    if (!fp)
        return count;
    char line[8192];
    while (count < cap && fgets(line, sizeof(line), fp)) {
        char *fields[4];
        char *cursor = line;
        size_t n = 0;
        for (; n < 4 && cursor; n++)
            fields[n] = strsep(&cursor, ":");
        if (n < 4 || fields[2][0] == '\0')
            continue;
        char *gid_end = NULL;
        unsigned long gid = strtoul(fields[2], &gid_end, 10);
        if ((*gid_end != '\0' && *gid_end != '\n') || gid > UINT32_MAX - 1 || !group_lists_member(fields[3], user))
            continue;
        bool listed = false;
        for (size_t i = 0; i < count && !listed; i++)
            listed = groups[i] == (gid_t)gid;
        if (!listed)
            groups[count++] = (gid_t)gid;
    }
    fclose(fp);
    return count;
}

static bool ensure_runtime_dir(const login_account_t *account, char *path, size_t cap) {
    if (snprintf(path, cap, "/run/user/%u", (unsigned)account->uid) >= (int)cap)
        return false;
    if ((mkdir("/run/user", 0755) < 0 && errno != EEXIST) || (mkdir(path, 0700) < 0 && errno != EEXIST))
        return false;
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        return false;
    bool ok = fchown(fd, account->uid, account->gid) == 0 && fchmod(fd, 0700) == 0;
    close(fd);
    return ok;
}

static int exec_account_shell(const char *user, const login_account_t *account, char **env, size_t count) {
    gid_t groups[LOGIN_GROUPS_MAX];
    size_t ngroups = account_groups(user, account->gid, groups, LOGIN_GROUPS_MAX);
    if (isatty(STDIN_FILENO) && fchown(STDIN_FILENO, account->uid, account->gid) < 0)
        return -errno;
    if (setgroups(ngroups, groups) < 0 || setgid(account->gid) < 0 || setuid(account->uid) < 0)
        return -errno;
    if (chdir(account->home) < 0 && chdir("/") < 0)
        return -errno;
    static char shell_env[PATH_MAX + 8];
    static char path_env[sizeof(TRUSTED_PATH) + sizeof(g_daemon.host_cmd_dir) + 8];
    if (snprintf(shell_env, sizeof(shell_env), "SHELL=%s", account->shell) >= (int)sizeof(shell_env) ||
        snprintf(path_env, sizeof(path_env), "PATH=" TRUSTED_PATH ":%s", g_daemon.host_cmd_dir) >=
            (int)sizeof(path_env))
        return -ENAMETOOLONG;
    env[count++] = shell_env;
    env[count++] = path_env;
    env[count] = NULL;
    char argv0[NAME_MAX + 2];
    if (snprintf(argv0, sizeof(argv0), "-%s", strrchr(account->shell, '/') + 1) >= (int)sizeof(argv0))
        return -ENAMETOOLONG;
    char *const argv[] = {argv0, NULL};
    execve(account->shell, argv, env);
    return -errno;
}

int login_exec(const char *user, char *const envp[], bool managed) {
    static login_account_t account;
    int rc = read_account(user, &account);
    if (rc < 0)
        return rc;
    char *env[LOGIN_ENV_MAX + 8];
    size_t count = login_environment(envp, env, LOGIN_ENV_MAX);
    static char home_env[PATH_MAX + 8];
    static char user_env[LOGIN_USER_CAP + 8];
    static char logname_env[LOGIN_USER_CAP + 16];
    static char runtime_env[64];
    char runtime_dir[32];
    if (snprintf(home_env, sizeof(home_env), "HOME=%s", account.home) >= (int)sizeof(home_env) ||
        snprintf(user_env, sizeof(user_env), "USER=%s", user) >= (int)sizeof(user_env) ||
        snprintf(logname_env, sizeof(logname_env), "LOGNAME=%s", user) >= (int)sizeof(logname_env))
        return -ENAMETOOLONG;
    env[count++] = home_env;
    env[count++] = user_env;
    env[count++] = logname_env;
    if (ensure_runtime_dir(&account, runtime_dir, sizeof(runtime_dir)) && managed &&
        snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir) < (int)sizeof(runtime_env))
        env[count++] = runtime_env;
    static char path_env[] = "PATH=" TRUSTED_PATH;
    env[count] = path_env;
    env[count + 1] = NULL;
    char *const argv[] = {"login", "-p", "-f", (char *)user, NULL};
    for (size_t i = 0; i < sizeof(login_paths) / sizeof(login_paths[0]); i++) {
        execve(login_paths[i], argv, env);
        if (errno != ENOENT && errno != ENOTDIR)
            return -errno;
    }
    return managed ? exec_account_shell(user, &account, env, count) : -ENOENT;
}

bool login_available(void) {
    for (size_t i = 0; i < sizeof(login_paths) / sizeof(login_paths[0]); i++) {
        int fd = fs_open_in_root(login_paths[i] + 1, O_PATH, 0);
        if (fd < 0)
            continue;
        struct stat st;
        bool ok = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & 0111);
        close(fd);
        if (ok)
            return true;
    }
    return false;
}

int sessions_anchor_main(void) {
    int fd = open("/proc/self/cgroup", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return 1;
    char membership[PATH_MAX];
    ssize_t n = read(fd, membership, sizeof(membership) - 1);
    close(fd);
    if (n <= 0)
        return 1;
    membership[n] = '\0';
    char *group = strstr(membership, "0::");
    if (!group)
        return 1;
    group += 3;
    group[strcspn(group, "\n")] = '\0';
    char service[PATH_MAX];
    if (snprintf(service, sizeof(service), "/sys/fs/cgroup%s", group) >= (int)sizeof(service))
        return 1;
    int dir = open(service, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir < 0)
        return 1;
    if ((mkdirat(dir, "agent", 0755) < 0 && errno != EEXIST) ||
        (mkdirat(dir, "sessions", 0755) < 0 && errno != EEXIST) ||
        write_file(dir, "agent/cgroup.procs", "0", 0, 0) < 0) {
        close(dir);
        return 1;
    }
    int controllers_fd = openat(dir, "cgroup.controllers", O_RDONLY | O_CLOEXEC);
    char controllers[512];
    ssize_t len = controllers_fd >= 0 ? read(controllers_fd, controllers, sizeof(controllers) - 1) : -1;
    if (controllers_fd >= 0)
        close(controllers_fd);
    if (len > 0) {
        controllers[len] = '\0';
        char *save = NULL;
        for (char *name = strtok_r(controllers, " \n", &save); name; name = strtok_r(NULL, " \n", &save)) {
            char request[64];
            if (snprintf(request, sizeof(request), "+%s", name) < (int)sizeof(request))
                (void)write_file(dir, "cgroup.subtree_control", request, 0, 0);
        }
    }
    close(dir);

    sigset_t stop;
    sigemptyset(&stop);
    sigaddset(&stop, SIGTERM);
    sigaddset(&stop, SIGINT);
    sigaddset(&stop, SIGHUP);
    sigprocmask(SIG_BLOCK, &stop, NULL);

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", GUEST_NOTIFY_SOCKET);
    int sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    static const char message[] = "X_SWIPTERM_SESSIONS=1";
    if (sock < 0 ||
        sendto(sock, message, sizeof(message) - 1, MSG_NOSIGNAL, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        return 1;
    close(sock);

    int sig = 0;
    while (sigwait(&stop, &sig) != 0) {}
    return 0;
}
