#include "swiptermd.h"

#include <stddef.h>
#include <unistd.h>

int main(int argc, char **argv) {
    (void)argc;
    static const char *const guest_modprobe[] = {
        "/sbin/modprobe",
        "/usr/sbin/modprobe",
        "/bin/modprobe",
        "/usr/bin/modprobe",
        "/run/current-system/sw/bin/modprobe",
    };
    if (chroot(GUEST_ROOT) < 0 || chdir("/") < 0)
        return 1;
    for (size_t i = 0; i < sizeof(guest_modprobe) / sizeof(guest_modprobe[0]); i++)
        execv(guest_modprobe[i], argv);
    return 1;
}
