#include "swiptermd.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sched.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static int g_host_net = -1;
static int g_guest_net = -1;

static int netlink_send_req(int nl, struct nlmsghdr *nlh) {
    static uint32_t seq = 1;
    nlh->nlmsg_seq = seq++;
    nlh->nlmsg_pid = 0;
    nlh->nlmsg_flags |= NLM_F_REQUEST | NLM_F_ACK;

    if (send(nl, nlh, nlh->nlmsg_len, 0) < 0)
        return -errno;

    uint8_t resp[4096];
    ssize_t n = recv(nl, resp, sizeof(resp), 0);
    if (n < 0)
        return -errno;
    if ((size_t)n < sizeof(struct nlmsghdr))
        return -EIO;
    struct nlmsghdr *rh = (struct nlmsghdr *)resp;
    if (rh->nlmsg_type == NLMSG_ERROR) {
        struct nlmsgerr *ne = NLMSG_DATA(rh);
        if (ne->error != 0 && ne->error != -EEXIST)
            return ne->error;
    }
    return 0;
}

static void rta_add(struct nlmsghdr *nlh, size_t maxlen, int type, const void *data, size_t len) {
    size_t rta_len = RTA_LENGTH(len);
    if (NLMSG_ALIGN(nlh->nlmsg_len) + RTA_ALIGN(rta_len) > maxlen)
        return;
    struct rtattr *rta = (struct rtattr *)((uint8_t *)nlh + NLMSG_ALIGN(nlh->nlmsg_len));
    rta->rta_type = (unsigned short)type;
    rta->rta_len = (unsigned short)rta_len;
    memcpy(RTA_DATA(rta), data, len);
    nlh->nlmsg_len = (uint32_t)(NLMSG_ALIGN(nlh->nlmsg_len) + RTA_ALIGN(rta_len));
}

static void net_add_addr(int nl, unsigned ifindex, int family, const char *ip_str, uint8_t prefixlen) {
    struct {
        struct nlmsghdr nlh;
        struct ifaddrmsg ifa;
        uint8_t attrs[128];
    } req;
    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(req.ifa));
    req.nlh.nlmsg_type = RTM_NEWADDR;
    req.nlh.nlmsg_flags = NLM_F_CREATE | NLM_F_REPLACE;
    req.ifa.ifa_family = (uint8_t)family;
    req.ifa.ifa_prefixlen = prefixlen;
    req.ifa.ifa_index = ifindex;
    req.ifa.ifa_scope = RT_SCOPE_UNIVERSE;

    uint8_t addr_bytes[16];
    size_t addr_len = (family == AF_INET) ? 4 : 16;
    if (inet_pton(family, ip_str, addr_bytes) != 1)
        return;
    rta_add(&req.nlh, sizeof(req), IFA_LOCAL, addr_bytes, addr_len);
    rta_add(&req.nlh, sizeof(req), IFA_ADDRESS, addr_bytes, addr_len);
    (void)netlink_send_req(nl, &req.nlh);
}

static void net_add_default_route(int nl, unsigned ifindex, int family, const char *gw_str) {
    struct {
        struct nlmsghdr nlh;
        struct rtmsg rtm;
        uint8_t attrs[128];
    } req;
    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(req.rtm));
    req.nlh.nlmsg_type = RTM_NEWROUTE;
    req.nlh.nlmsg_flags = NLM_F_CREATE | NLM_F_REPLACE;
    req.rtm.rtm_family = (uint8_t)family;
    req.rtm.rtm_table = RT_TABLE_MAIN;
    req.rtm.rtm_protocol = RTPROT_BOOT;
    req.rtm.rtm_scope = RT_SCOPE_UNIVERSE;
    req.rtm.rtm_type = RTN_UNICAST;

    uint8_t gw_bytes[16];
    size_t gw_len = (family == AF_INET) ? 4 : 16;
    if (inet_pton(family, gw_str, gw_bytes) != 1)
        return;
    rta_add(&req.nlh, sizeof(req), RTA_GATEWAY, gw_bytes, gw_len);
    uint32_t oif = ifindex;
    rta_add(&req.nlh, sizeof(req), RTA_OIF, &oif, sizeof(oif));
    (void)netlink_send_req(nl, &req.nlh);
}

static void net_link_up(int ioctl_fd, const char *ifname) {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(ioctl_fd, SIOCGIFFLAGS, &ifr) == 0) {
        ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
        (void)ioctl(ioctl_fd, SIOCSIFFLAGS, &ifr);
    }
}

static int net_move_link(int nl, unsigned ifindex, int netns_fd) {
    struct {
        struct nlmsghdr nlh;
        struct ifinfomsg ifi;
        uint8_t attrs[32];
    } req;
    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(req.ifi));
    req.nlh.nlmsg_type = RTM_NEWLINK;
    req.ifi.ifi_family = AF_UNSPEC;
    req.ifi.ifi_index = (int)ifindex;
    uint32_t fd = (uint32_t)netns_fd;
    rta_add(&req.nlh, sizeof(req), IFLA_NET_NS_FD, &fd, sizeof(fd));
    return netlink_send_req(nl, &req.nlh);
}

static int netlink_open(void) {
    int nl = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (nl < 0)
        return -errno;
    struct sockaddr_nl sa;
    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    if (bind(nl, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        int err = -errno;
        close(nl);
        return err;
    }
    return nl;
}

static void configure_guest_links(void) {
    int nl = netlink_open();
    int ioctl_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (nl < 0 || ioctl_fd < 0) {
        boot_err_log("configure guest network", nl < 0 ? nl : -errno);
        if (nl >= 0)
            close(nl);
        if (ioctl_fd >= 0)
            close(ioctl_fd);
        return;
    }

    unsigned lo_idx = if_nametoindex("lo");
    if (lo_idx > 0) {
        net_link_up(ioctl_fd, "lo");
        net_add_addr(nl, lo_idx, AF_INET, "127.0.0.1", 8);
    }

    unsigned eth_idx = if_nametoindex("eth0");
    if (eth_idx > 0) {
        net_link_up(ioctl_fd, "eth0");
        (void)write_file(AT_FDCWD, "/proc/sys/net/ipv4/conf/eth0/route_localnet", "1", 0, 0);
        net_add_addr(nl, eth_idx, AF_INET, GUEST_IPV4_ADDR, GUEST_IPV4_PREFIXLEN);
        net_add_default_route(nl, eth_idx, AF_INET, GUEST_IPV4_GATEWAY);
        net_add_addr(nl, eth_idx, AF_INET6, GUEST_IPV6_ADDR, GUEST_IPV6_PREFIXLEN);
        net_add_default_route(nl, eth_idx, AF_INET6, GUEST_IPV6_GATEWAY);
    }

    close(ioctl_fd);
    close(nl);
}

int network_guest_fd(void) {
    return g_guest_net;
}

int network_enter_guest(void) {
    return setns(g_guest_net, CLONE_NEWNET) < 0 ? -errno : 0;
}

void network_leave_guest(void) {
    if (setns(g_host_net, CLONE_NEWNET) < 0)
        boot_err_log("return to the host network namespace", -errno);
}

int guest_socket(int family, int type, int protocol) {
    int rc = network_enter_guest();
    if (rc < 0)
        return rc;
    int fd = socket(family, type, protocol);
    int err = fd < 0 ? -errno : 0;
    network_leave_guest();
    return fd < 0 ? err : fd;
}

int network_setup(void) {
    g_host_net = open("/proc/self/ns/net", O_RDONLY | O_CLOEXEC);
    if (g_host_net < 0)
        return -errno;
    int nl = netlink_open();
    if (nl < 0)
        return nl;
    unsigned eth_idx = if_nametoindex("eth0");
    int rc = unshare(CLONE_NEWNET) < 0 ? -errno : 0;
    if (rc == 0) {
        g_guest_net = open("/proc/self/ns/net", O_RDONLY | O_CLOEXEC);
        if (g_guest_net < 0)
            rc = -errno;
        network_leave_guest();
    }
    if (rc == 0 && eth_idx > 0)
        rc = net_move_link(nl, eth_idx, g_guest_net);
    close(nl);
    if (rc < 0)
        return rc;
    rc = network_enter_guest();
    if (rc < 0)
        return rc;
    configure_guest_links();
    network_leave_guest();
    return 0;
}
