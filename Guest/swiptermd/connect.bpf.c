typedef unsigned short __u16;
typedef unsigned int __u32;
typedef unsigned long long __u64;

enum {
    SOCK_STREAM = 1,
    BPF_FUNC_sk_lookup_tcp = 84,
    BPF_FUNC_sk_release = 86,
};

#define BPF_F_CURRENT_NETNS (-1L)

struct bpf_sock;

struct bpf_sock_addr {
    __u32 user_family;
    __u32 user_ip4;
    __u32 user_ip6[4];
    __u32 user_port;
    __u32 family;
    __u32 type;
    __u32 protocol;
    __u32 msg_src_ip4;
    __u32 msg_src_ip6[4];
    struct bpf_sock *sk;
};

struct bpf_sock_tuple {
    union {
        struct {
            __u32 saddr;
            __u32 daddr;
            __u16 sport;
            __u16 dport;
        } ipv4;
        struct {
            __u32 saddr[4];
            __u32 daddr[4];
            __u16 sport;
            __u16 dport;
        } ipv6;
    };
};

static struct bpf_sock *(*const bpf_sk_lookup_tcp)(
    void *ctx, struct bpf_sock_tuple *tuple, __u32 tuple_size, __u64 netns, __u64 flags) =
    (void *)BPF_FUNC_sk_lookup_tcp;
static long (*const bpf_sk_release)(void *sock) = (void *)BPF_FUNC_sk_release;

static __attribute__((always_inline)) int has_ipv4_listener(struct bpf_sock_addr *ctx, __u32 daddr, __u16 dport) {
    struct bpf_sock_tuple tuple = {};
    tuple.ipv4.daddr = daddr;
    tuple.ipv4.dport = dport;
    struct bpf_sock *sk = bpf_sk_lookup_tcp(ctx, &tuple, sizeof(tuple.ipv4), BPF_F_CURRENT_NETNS, 0);
    if (sk) {
        bpf_sk_release(sk);
        return 1;
    }
    __builtin_memset(&tuple, 0, sizeof(tuple));
    tuple.ipv6.daddr[2] = __builtin_bswap32(0x0000ffffU);
    tuple.ipv6.daddr[3] = daddr;
    tuple.ipv6.dport = dport;
    sk = bpf_sk_lookup_tcp(ctx, &tuple, sizeof(tuple.ipv6), BPF_F_CURRENT_NETNS, 0);
    if (sk) {
        bpf_sk_release(sk);
        return 1;
    }
    return 0;
}

__attribute__((section("connect4"), used)) int swipterm_connect4(struct bpf_sock_addr *ctx) {
    if (ctx->type != SOCK_STREAM || ctx->user_ip4 != __builtin_bswap32(0x7f000001U))
        return 1;
    if (has_ipv4_listener(ctx, ctx->user_ip4, (__u16)ctx->user_port))
        return 1;
    ctx->user_ip4 = __builtin_bswap32(0x64735c01U);
    return 1;
}

__attribute__((section("connect6"), used)) int swipterm_connect6(struct bpf_sock_addr *ctx) {
    if (ctx->type != SOCK_STREAM)
        return 1;
    __u32 w0 = ctx->user_ip6[0];
    __u32 w1 = ctx->user_ip6[1];
    __u32 w2 = ctx->user_ip6[2];
    __u32 w3 = ctx->user_ip6[3];
    if (w0 == 0 && w1 == 0 && w2 == 0 && w3 == __builtin_bswap32(1U)) {
        struct bpf_sock_tuple tuple = {};
        tuple.ipv6.daddr[3] = w3;
        tuple.ipv6.dport = (__u16)ctx->user_port;
        struct bpf_sock *sk = bpf_sk_lookup_tcp(ctx, &tuple, sizeof(tuple.ipv6), BPF_F_CURRENT_NETNS, 0);
        if (sk) {
            bpf_sk_release(sk);
            return 1;
        }
        *(volatile __u32 *)&ctx->user_ip6[0] = __builtin_bswap32(0xfd537769U);
        *(volatile __u32 *)&ctx->user_ip6[1] = __builtin_bswap32(0x70740000U);
        *(volatile __u32 *)&ctx->user_ip6[2] = 0;
        *(volatile __u32 *)&ctx->user_ip6[3] = __builtin_bswap32(1U);
        return 1;
    }
    if (w0 == 0 && w1 == 0 && w2 == __builtin_bswap32(0x0000ffffU) && w3 == __builtin_bswap32(0x7f000001U)) {
        if (has_ipv4_listener(ctx, w3, (__u16)ctx->user_port))
            return 1;
        *(volatile __u32 *)&ctx->user_ip6[3] = __builtin_bswap32(0x64735c01U);
    }
    return 1;
}
