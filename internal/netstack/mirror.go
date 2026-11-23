package netstack

import (
	"context"
	"errors"
	"net"
	"net/netip"
	"sync"
	"sync/atomic"
	"time"

	E "github.com/sagernet/sing/common/exceptions"
	"github.com/swipt-automation/SwiptermKit/internal/vmm"

	"golang.org/x/sys/unix"
)

type mirrorKey struct {
	inet6 bool
	port  uint16
}

type mirrorBinding struct {
	bind   netip.Addr
	target netip.AddrPort
	bridge bool
}

type mirrorListener struct {
	listener *net.TCPListener
	bind     netip.Addr
	binding  atomic.Pointer[mirrorBinding]
}

type portMirror struct {
	stack   *Stack
	access  sync.Mutex
	closed  bool
	guest   map[netip.AddrPort]struct{}
	hosts   map[mirrorKey]*mirrorListener
	workers sync.WaitGroup
}

func newPortMirror(stack *Stack) *portMirror {
	return &portMirror{
		stack: stack,
		guest: make(map[netip.AddrPort]struct{}),
		hosts: make(map[mirrorKey]*mirrorListener),
	}
}

func (m *portMirror) update(listeners []vmm.Listener, reset bool) {
	m.access.Lock()
	defer m.access.Unlock()
	if m.closed {
		return
	}
	ports := make(map[uint16]struct{})
	if reset {
		for address := range m.guest {
			ports[address.Port()] = struct{}{}
		}
		clear(m.guest)
	}
	for _, listener := range listeners {
		if listener.Added {
			m.guest[listener.Address] = struct{}{}
		} else {
			delete(m.guest, listener.Address)
		}
		ports[listener.Address.Port()] = struct{}{}
	}
	for port := range ports {
		m.reconcileLocked(port)
	}
}

func (m *portMirror) reconcileLocked(port uint16) {
	var (
		inet4Network  bool
		inet6Any      bool
		inet6Network  bool
		inet6Loopback bool
		inet4Loopback netip.Addr
	)
	for address := range m.guest {
		if address.Port() != port {
			continue
		}
		addr := address.Addr()
		switch {
		case addr == netip.IPv4Unspecified() || addr == inet4Guest:
			inet4Network = true
		case addr == netip.IPv6Unspecified():
			inet6Any = true
		case addr == inet6Guest:
			inet6Network = true
		case addr == netip.IPv6Loopback():
			inet6Loopback = true
		case addr.Is4() && addr.IsLoopback() && (!inet4Loopback.IsValid() || addr.Less(inet4Loopback)):
			inet4Loopback = addr
		}
	}
	var inet4, inet6 mirrorBinding
	switch {
	case inet4Network:
		inet4 = mirrorBinding{bind: netip.IPv4Unspecified(), target: netip.AddrPortFrom(inet4Guest, port)}
	case inet6Any:
		inet4 = mirrorBinding{bind: netip.IPv4Unspecified(), target: netip.AddrPortFrom(inet6Guest, port)}
	case inet4Loopback.IsValid():
		inet4 = mirrorBinding{bind: netip.AddrFrom4([4]byte{127, 0, 0, 1}), target: netip.AddrPortFrom(inet4Loopback, port), bridge: true}
	}
	switch {
	case inet6Any || inet6Network:
		inet6 = mirrorBinding{bind: netip.IPv6Unspecified(), target: netip.AddrPortFrom(inet6Guest, port)}
	case inet6Loopback:
		inet6 = mirrorBinding{bind: netip.IPv6Loopback(), target: netip.AddrPortFrom(netip.IPv6Loopback(), port), bridge: true}
	}
	m.applyLocked(mirrorKey{inet6: false, port: port}, inet4)
	m.applyLocked(mirrorKey{inet6: true, port: port}, inet6)
}

func (m *portMirror) applyLocked(key mirrorKey, binding mirrorBinding) {
	current := m.hosts[key]
	if current != nil && current.bind == binding.bind {
		current.binding.Store(&binding)
		return
	}
	if current != nil {
		current.listener.Close()
		delete(m.hosts, key)
	}
	if !binding.bind.IsValid() {
		return
	}
	network := "tcp4"
	if key.inet6 {
		network = "tcp6"
	}
	listener, err := net.ListenTCP(network, net.TCPAddrFromAddrPort(netip.AddrPortFrom(binding.bind, key.port)))
	if err != nil {
		m.stack.logger.Debug("mirror: listen ", netip.AddrPortFrom(binding.bind, key.port), ": ", err)
		return
	}
	entry := &mirrorListener{listener: listener, bind: binding.bind}
	entry.binding.Store(&binding)
	m.hosts[key] = entry
	m.workers.Go(func() {
		m.loopAccept(entry)
	})
}

func (m *portMirror) loopAccept(entry *mirrorListener) {
	for {
		conn, err := entry.listener.AcceptTCP()
		if err != nil {
			if errors.Is(err, net.ErrClosed) {
				return
			}
			time.Sleep(50 * time.Millisecond)
			continue
		}
		binding := entry.binding.Load()
		m.workers.Go(func() {
			m.forward(conn, binding)
		})
	}
}

func (m *portMirror) forward(conn *net.TCPConn, binding *mirrorBinding) {
	if binding.bridge {
		err := m.bridge(conn, binding.target)
		if err != nil {
			m.stack.logger.Debug("mirror: bridge ", binding.target, ": ", err)
		}
		return
	}
	source := inet4Gateway
	if binding.target.Addr().Is6() {
		source = inet6Gateway
	}
	ctx, cancel := context.WithTimeout(m.stack.ctx, tcpConnectTimeout)
	goConn, err := m.stack.stack.DialTCP(ctx, source, binding.target)
	cancel()
	if err != nil {
		conn.Close()
		m.stack.logger.Debug("mirror: connect ", binding.target, ": ", err)
		return
	}
	m.stack.connections.splice(m.stack.ctx, goConn, conn, nil)
}

func (m *portMirror) bridge(conn *net.TCPConn, target netip.AddrPort) error {
	raw, err := conn.SyscallConn()
	if err != nil {
		conn.Close()
		return err
	}
	fd := -1
	var dupErr error
	err = raw.Control(func(descriptor uintptr) {
		fd, dupErr = unix.FcntlInt(descriptor, unix.F_DUPFD_CLOEXEC, 0)
	})
	conn.Close()
	err = E.Errors(err, dupErr)
	if err != nil {
		return err
	}
	err = m.stack.network.Bridge(target.Addr().Is6(), fd, target.Port())
	if err != nil {
		unix.Close(fd)
	}
	return err
}

func (m *portMirror) close() {
	m.access.Lock()
	m.closed = true
	for key, entry := range m.hosts {
		entry.listener.Close()
		delete(m.hosts, key)
	}
	m.access.Unlock()
	m.workers.Wait()
}
