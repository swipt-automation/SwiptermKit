<h1 align="center">SwiptermKit</h1>

<p align="center">A Linux engine for iOS.</p>

## Requirements

- iOS 17.0+

## Architecture

- VMM: A high-performance user-space system emulator that boots unmodified Linux kernels without JIT or hypervisor entitlements, and runs guest code through a native ARM64 JIT when the host has prepared executable memory.
- Root Image Builder: Builds bootable ext4 root images from distribution rootfs archives and kernel packages on the host.
- Guest Runtime (`swiptermd`): A lightweight initramfs and guest agent handling terminal sessions, file transfer, host integration, and system init handoff.
- Network Stack: High-performance user-space NAT networking and automatic port mirroring built on [sing-tun](https://github.com/SagerNet/sing-tun).

## License

```text
Copyright (C) 2026 by nekohasekai <i@sekai.icu>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

In addition, no derivative work may use the name or imply association
with this application without prior consent.
```
