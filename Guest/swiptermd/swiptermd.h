#ifndef SWIPTERMD_INTERNAL_H
#define SWIPTERMD_INTERNAL_H

#define _GNU_SOURCE

#include "guest_ipc.h"
#include "protocol.h"
#include "wire_format.h"

#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>

#define GUEST_ROOT "/mnt/root"
#define GUEST_NOTIFY_SOCKET GUEST_RUN_HOST "/notify"
#define GUEST_WAYLAND_SOCKET GUEST_RUN_HOST "/wayland-0"
#define GUEST_TOOL_PATH GUEST_RUN_HOST "/swiptermd"
#define GUEST_IPV4_GATEWAY "100.115.92.1"
#define GUEST_IPV4_ADDR "100.115.92.2"
#define GUEST_IPV4_PREFIXLEN 30
#define GUEST_IPV6_GATEWAY "fd53:7769:7074::1"
#define GUEST_IPV6_ADDR "fd53:7769:7074::2"
#define GUEST_IPV6_PREFIXLEN 126
#define GUEST_DEFAULT_RESOLV_CONF "nameserver " GUEST_IPV4_GATEWAY "\nnameserver " GUEST_IPV6_GATEWAY "\n"
#define SESSIONS_ANCHOR_COMMAND "sessions-anchor"
#define SESSIONS_READY_MESSAGE "X_SWIPTERM_SESSIONS=1"
#define MANAGED_INIT_NAME "init"
#define TRUSTED_PATH "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/run/current-system/sw/bin"
#define GPU_VENUS_ENV_NAME "VN_DEBUG"
#define GPU_VENUS_ENV_VALUE "no_abort"
#define GPU_VENUS_ENV GPU_VENUS_ENV_NAME "=" GPU_VENUS_ENV_VALUE
#define LOGIN_USER_CAP 33
#define VIRTIO_MMIO_ADDR(slot) (0x0a000000U + (unsigned)(slot) * 0x200U)

enum {
    VIRTIO_SLOT_BOOT_COUNT = 4,
    VIRTIO_SLOT_RNG = 4,
    VIRTIO_SLOT_FS_BASE = 5,
    VIRTIO_SLOT_FS_COUNT = 4,
    VIRTIO_SLOT_DISK_BASE = 9,
    VIRTIO_SLOT_DISK_COUNT = 4,
    VIRTIO_SLOT_GPU = 13,
};

typedef struct ev_node ev_node_t;
typedef void (*ev_cb_t)(ev_node_t *node, uint32_t events);

struct ev_node {
    int fd;
    uint32_t events;
    ev_cb_t cb;
    void *ctx;
    bool registered;
};

typedef struct daemon_state {
    int epfd;
    int ctrl_fd;
    int data_listen_fd;
    int root_fd;
    uint64_t session_nonce;
    uint32_t next_req_id;

    uint32_t init_mode;
    char hostname[256];
    char host_cmd_dir[512];

    bool init_started;
    bool gpu;
    bool stopping;
    bool shutdown_requested;
    bool reboot_requested;
    uint32_t ready_mask;
    pid_t container_init_pid;
    int container_pid_ns_fd;
    int container_mnt_ns_fd;
    int container_uts_ns_fd;
    int container_ipc_ns_fd;
    int container_cgroup_ns_fd;
} daemon_state_t;

extern daemon_state_t g_daemon;

int ev_add(ev_node_t *node, int fd, uint32_t events, ev_cb_t cb, void *ctx);
int ev_mod(ev_node_t *node, uint32_t events);
void ev_del(ev_node_t *node);

int set_nonblock(int fd, bool enabled);
int write_file(int dir_fd, const char *path, const char *text, int flags, mode_t mode);
void close_other_fds(const int *keep, int nkeep);
void redirect_stdio(int fd);
_Noreturn void child_exit_error(int fd, int32_t err);
char **strv_decode(swiptermd_buf_r_t *r);
void strv_free(char **vec);
int namespace_open(pid_t pid, const char *name);
void console_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
void boot_err_log(const char *step, int rc);
int vsock_connect_stream(uint32_t role, uint32_t resource_id, uint64_t token);
int listen_unix_socket(const char *path, ev_node_t *ev, ev_cb_t cb);

int ctrl_send_frame(uint16_t opcode, uint32_t req_id, const void *body, uint32_t body_len);
int ctrl_send_err_reply(uint16_t opcode, uint32_t req_id, int32_t err);
uint32_t ctrl_alloc_req_id(void);
void ready_announce(uint32_t level);
int virtio_mmio_bind(unsigned addr);
void virtio_mmio_unbind(unsigned addr);
void shutdown_begin(bool reboot);
void shutdown_check_done(void);

int storage_mount_root(void);
int storage_check(const char *device);
void storage_teardown(void);

int network_setup(void);
int network_guest_fd(void);
int network_enter_guest(void);
void network_leave_guest(void);
int guest_socket(int family, int type, int protocol);

int cgroup_setup(void);
int container_assemble(void);
int container_enter(bool disk);
int disk_procs_fd(void);
int container_mount_api(void);
int dev_populate(const char *dev);
void cgroup_enable_controllers(int dir_fd);
bool cgroup_membership(const char *proc_cgroup, char *group, size_t cap);
int sessions_procs_fd(void);
void sessions_register(pid_t sender);
void sessions_shutdown(void);
int device_permit(unsigned major_number, unsigned minor_number);
void device_revoke(unsigned major_number, unsigned minor_number);

int notify_setup(void);
void notify_close(void);
int notify_signals_level(void);

bool login_name_valid(const char *name);
int login_exec(const char *user, char *const envp[], bool managed);
int shell_exec(const char *user, char *const envp[]);
bool login_available(void);
int sessions_anchor_main(void);

void ns_handle_configure_init(uint32_t req_id, const uint8_t *body, uint32_t len);
void ns_handle_start_init(uint32_t req_id, const uint8_t *body, uint32_t len);
void ns_init_exited(int status);
void ns_request_stop(bool reboot);
void ns_kill_container(void);
void ns_close_namespaces(void);
bool ns_init_executable(char *name, size_t cap);
bool ns_init_is_systemd(void);
_Noreturn void managed_init_main(void);

int readiness_start(void);
void readiness_stop(void);

typedef struct launch_target {
    int net_ns_fd;
    int ipc_ns_fd;
    int uts_ns_fd;
    int mnt_ns_fd;
    int pid_ns_fd;
    int cgroup_ns_fd;
    int procs_fd;
} launch_target_t;

typedef struct disk_sandbox disk_sandbox_t;

void launch_target_close(launch_target_t *target);
int pty_open_master(int *pts_num_out);
void proc_pty_shutdown(void);
void proc_handle_disk_spawn(uint32_t req_id, const uint8_t *body, uint32_t len);
void proc_handle_login(uint32_t req_id, const uint8_t *body, uint32_t len);
void proc_handle_signal(uint32_t req_id, const uint8_t *body, uint32_t len);
void proc_bridge_resize(uint32_t invocation_id);
int proc_run_in_container(char *const argv[], void (*exit_handler)(int status));
void pty_handle_create(uint32_t req_id, const uint8_t *body, uint32_t len);
void pty_handle_resize(uint32_t req_id, const uint8_t *body, uint32_t len);
void pty_handle_close(uint32_t req_id, const uint8_t *body, uint32_t len);

void fs_handle_stat(uint32_t req_id, const uint8_t *body, uint32_t len);
void fs_handle_read(uint32_t req_id, const uint8_t *body, uint32_t len);
void fs_handle_write(uint32_t req_id, const uint8_t *body, uint32_t len);
void fs_handle_mkdir(uint32_t req_id, const uint8_t *body, uint32_t len);
void fs_handle_mount_virtiofs(uint32_t req_id, const uint8_t *body, uint32_t len);
void fs_unmount_all(void);
int fs_normalize_path(const char *in, char *out, size_t cap);
int fs_prepare_mountpoint(const char *guest_path, char norm[PATH_MAX]);
int fs_open_in_root(const char *rel, uint64_t flags, uint64_t mode);
char *fs_read_root_file(const char *rel, struct stat *st);
int fs_open_parent(const char *guest_path, char *name_out, size_t name_cap);
int fs_write_file_jailed(const char *guest_path, const uint8_t *data, size_t len, uint32_t mode, bool sync);
int fs_mkdir_p_jailed(const char *guest_path, uint32_t mode);

void disk_handle_attach(uint32_t req_id, const uint8_t *body, uint32_t len);
void disk_handle_detach(uint32_t req_id, const uint8_t *body, uint32_t len);
void disk_handle_mount(uint32_t req_id, const uint8_t *body, uint32_t len);
void disk_handle_unmount(uint32_t req_id, const uint8_t *body, uint32_t len);
int disk_sandbox_open(uint32_t disk_id, const char *hostname, launch_target_t *target, disk_sandbox_t **sandbox_out);
void disk_sandbox_close(disk_sandbox_t *sandbox);
void disk_shutdown(void);

void usage_setup(void);
void usage_shutdown(void);
void usage_handle_read(uint32_t req_id);

int host_cmd_setup(const char *guest_dir);
int host_cmd_borrow_stdio(uint32_t invocation_id, int fds[3]);
void host_cmd_cuse_shutdown(void);
void host_cmd_handle_register(uint32_t req_id, const uint8_t *body, uint32_t len);
void host_cmd_handle_finish(const uint8_t *body, uint32_t len);
void host_cmd_handle_start_reply(uint32_t req_id, const uint8_t *body, uint32_t len);

bool cuse_available(void);
void cuse_handle_register_device(uint32_t req_id, const uint8_t *body, uint32_t len);
void cuse_handle_reply(uint16_t opcode, uint32_t req_id, const uint8_t *body, uint32_t len);

void sock_mirror_init(void);
void sock_mirror_scan_now(void);
void sock_mirror_handle_incoming(int vsock_fd, uint32_t resource_id);

int clipboard_setup(void);
void clipboard_shutdown(void);
void clipboard_handle_offer(const uint8_t *body, uint32_t len);

#endif
