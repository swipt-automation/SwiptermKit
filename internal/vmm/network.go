package vmm

/*
#cgo CFLAGS: -std=gnu2x -I${SRCDIR}/../../VMM/host
#cgo ios,!iossimulator LDFLAGS: -L${SRCDIR}/../../build/iphoneos
#cgo iossimulator LDFLAGS: -L${SRCDIR}/../../build/iphonesimulator
#cgo macos LDFLAGS: -L${SRCDIR}/../../build/macosx
#cgo darwin,!ios,!macos LDFLAGS: -L${SRCDIR}/../../build/macosx
#cgo LDFLAGS: -lvmm -larchive -lz
#include <errno.h>
#include "network.h"
*/
import "C"

import (
	"net/netip"
	"os"
	"syscall"
	"unsafe"

	E "github.com/sagernet/sing/common/exceptions"

	"golang.org/x/sys/unix"
)

const (
	MTU        = C.SWIPTERM_NETWORK_MTU
	HeaderSize = C.SWIPTERM_NETWORK_HEADER_SIZE
	MaxFrame   = C.SWIPTERM_NETWORK_MAX_FRAME

	FeatureGuestChecksum = uint64(C.SWIPTERM_NETWORK_F_GUEST_CSUM)
	FeatureGuestTSO4     = uint64(C.SWIPTERM_NETWORK_F_GUEST_TSO4)
	FeatureGuestTSO6     = uint64(C.SWIPTERM_NETWORK_F_GUEST_TSO6)
	FeatureGuestUSO4     = uint64(C.SWIPTERM_NETWORK_F_GUEST_USO4)
	FeatureGuestUSO6     = uint64(C.SWIPTERM_NETWORK_F_GUEST_USO6)
)

type Wake int

const (
	WakeTX    Wake = C.SWIPTERM_NETWORK_WAKE_TX
	WakeRX    Wake = C.SWIPTERM_NETWORK_WAKE_RX
	WakeEvent Wake = C.SWIPTERM_NETWORK_WAKE_EVENT
)

type Listener struct {
	Address netip.AddrPort
	Added   bool
}

type Network struct {
	handle *C.swipterm_network
	wakes  [C.SWIPTERM_NETWORK_WAKE_COUNT]*os.File
	raws   [C.SWIPTERM_NETWORK_WAKE_COUNT]syscall.RawConn
}

func NewNetwork() (*Network, error) {
	handle, err := C.swipterm_network_create()
	if handle == nil {
		return nil, E.Cause(err, "create network")
	}
	network := &Network{handle: handle}
	for wake := range network.wakes {
		fd, err := unix.Dup(int(C.swipterm_network_wake_fd(handle, C.swipterm_network_wake(wake))))
		if err != nil {
			network.Close()
			return nil, E.Cause(err, "duplicate network wake descriptor")
		}
		unix.CloseOnExec(fd)
		err = unix.SetNonblock(fd, true)
		if err != nil {
			unix.Close(fd)
			network.Close()
			return nil, E.Cause(err, "set network wake descriptor non-blocking")
		}
		network.wakes[wake] = os.NewFile(uintptr(fd), "swipterm-network")
		network.raws[wake], err = network.wakes[wake].SyscallConn()
		if err != nil {
			network.Close()
			return nil, err
		}
	}
	return network, nil
}

func (n *Network) Handle() uintptr {
	return uintptr(unsafe.Pointer(n.handle))
}

func (n *Network) Wait(wake Wake) error {
	var readErr error
	err := n.raws[wake].Read(func(fd uintptr) bool {
		var buffer [64]byte
		drained := false
		for {
			count, err := unix.Read(int(fd), buffer[:])
			switch {
			case count > 0:
				drained = true
			case err == unix.EINTR:
			case err == unix.EAGAIN:
				return drained
			default:
				readErr = err
				return true
			}
		}
	})
	if err != nil {
		return err
	}
	return readErr
}

func (n *Network) PopTX(arena []byte, lengths []uint32) int {
	return int(C.swipterm_network_pop_tx(n.handle, (*C.uint8_t)(unsafe.Pointer(unsafe.SliceData(arena))), C.size_t(len(arena)),
		(*C.uint32_t)(unsafe.Pointer(unsafe.SliceData(lengths))), C.size_t(len(lengths))))
}

func (n *Network) PushRX(frame []byte, flush bool) bool {
	return C.swipterm_network_push_rx(n.handle, (*C.uint8_t)(unsafe.Pointer(unsafe.SliceData(frame))), C.size_t(len(frame)), C.bool(flush)) != -C.EAGAIN
}

func (n *Network) Features() uint64 {
	return uint64(C.swipterm_network_features(n.handle))
}

func (n *Network) TakeListeners(listeners []Listener) ([]Listener, bool) {
	var (
		raw   [64]C.swipterm_network_listener
		reset C.bool
	)
	count := int(C.swipterm_network_take_listeners(n.handle, &raw[0], C.size_t(len(raw)), &reset))
	for _, entry := range raw[:count] {
		port := uint16(entry.port)
		var address netip.Addr
		if entry.family == 6 {
			address = netip.AddrFrom16(*(*[16]byte)(unsafe.Pointer(&entry.address)))
		} else {
			address = netip.AddrFrom4(*(*[4]byte)(unsafe.Pointer(&entry.address)))
		}
		listeners = append(listeners, Listener{
			Address: netip.AddrPortFrom(address, port),
			Added:   entry.action == C.SWIPTERM_NETWORK_LISTENER_ADDED,
		})
	}
	return listeners, bool(reset)
}

func (n *Network) Bridge(inet6 bool, fd int, port uint16) error {
	rc := C.swipterm_network_bridge(n.handle, C.bool(inet6), C.int(fd), C.uint16_t(port))
	if rc < 0 {
		return syscall.Errno(-rc)
	}
	return nil
}

func (n *Network) CloseWakes() {
	for _, file := range n.wakes {
		if file != nil {
			file.Close()
		}
	}
}

func (n *Network) Close() {
	n.CloseWakes()
	C.swipterm_network_release(n.handle)
}
