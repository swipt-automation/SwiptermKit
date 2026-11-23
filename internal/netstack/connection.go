package netstack

import (
	"context"
	"io"
	"net"
	"sync"
	"syscall"
	"time"

	"github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

const (
	tcpConnectTimeout = 75 * time.Second
	udpSendBuffer     = 256 << 10
)

type ConnectionManager struct {
	logger logger.ContextLogger
}

func (m *ConnectionManager) NewConnection(ctx context.Context, conn net.Conn, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	dialer := net.Dialer{Timeout: tcpConnectTimeout}
	remote, err := dialer.DialContext(ctx, N.NetworkTCP, destination.String())
	if err != nil {
		N.CloseOnHandshakeFailure(conn, onClose, err)
		m.logger.DebugContext(ctx, "open connection to ", destination, ": ", err)
		return
	}
	err = N.ReportConnHandshakeSuccess(conn, remote)
	if err != nil {
		remote.Close()
		N.CloseOnHandshakeFailure(conn, onClose, err)
		m.logger.DebugContext(ctx, "report handshake success for ", destination, ": ", err)
		return
	}
	m.splice(ctx, conn.(*tun.GoConn), remote, onClose)
}

func (m *ConnectionManager) splice(ctx context.Context, conn *tun.GoConn, remote net.Conn, onClose N.CloseHandlerFunc) {
	socket := &spliceSocket{conn: remote.(syscallCloser)}
	if conn.Splice(socket, tun.SpliceOptions{OnClose: m.closeHandler(ctx, conn, onClose)}) {
		return
	}
	go func() {
		err := bufio.CopyConn(ctx, conn, remote)
		m.closeHandler(ctx, conn, onClose)(err)
		remote.Close()
	}()
}

func (m *ConnectionManager) NewPacketConnection(ctx context.Context, conn N.PacketConn, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	remote, err := net.ListenUDP(N.NetworkUDP, nil)
	if err == nil {
		err = remote.SetWriteBuffer(udpSendBuffer)
		if err != nil {
			remote.Close()
		}
	}
	if err != nil {
		N.CloseOnHandshakeFailure(conn, onClose, err)
		m.logger.DebugContext(ctx, "listen packet connection for ", destination, ": ", err)
		return
	}
	err = N.ReportPacketConnHandshakeSuccess(conn, remote)
	if err != nil {
		remote.Close()
		N.CloseOnHandshakeFailure(conn, onClose, err)
		m.logger.DebugContext(ctx, "report packet handshake success for ", destination, ": ", err)
		return
	}
	socket := &spliceSocket{conn: remote}
	if conn.(*tun.UDPNatConn).Splice(socket, tun.SplicePacketOptions{
		SpliceOptions: tun.SpliceOptions{OnClose: m.closeHandler(ctx, conn, onClose)},
		Timeout:       udpTimeout,
	}) {
		return
	}
	go func() {
		err := bufio.CopyPacketConn(ctx, conn, bufio.NewPacketConn(remote))
		m.closeHandler(ctx, conn, onClose)(err)
		remote.Close()
	}()
}

func (m *ConnectionManager) closeHandler(ctx context.Context, conn io.Closer, onClose N.CloseHandlerFunc) N.CloseHandlerFunc {
	return func(err error) {
		if err != nil && !E.IsClosedOrCanceled(err) {
			m.logger.DebugContext(ctx, "connection closed: ", err)
		}
		if onClose != nil {
			onClose(err)
		}
		conn.Close()
	}
}

type syscallCloser interface {
	syscall.Conn
	io.Closer
}

type spliceSocket struct {
	conn   syscallCloser
	access sync.Mutex
	owner  io.Closer
	closed bool
}

func (s *spliceSocket) SyscallConn() (syscall.RawConn, error) {
	return s.conn.SyscallConn()
}

func (s *spliceSocket) Attach(owner io.Closer) (io.Closer, bool) {
	s.access.Lock()
	defer s.access.Unlock()
	if s.closed || s.owner != nil {
		return nil, false
	}
	s.owner = owner
	return s.conn, true
}

func (s *spliceSocket) Detach() {
	s.access.Lock()
	s.owner = nil
	closed := s.closed
	s.access.Unlock()
	if closed {
		s.conn.Close()
	}
}

func (s *spliceSocket) Close() error {
	s.access.Lock()
	s.closed = true
	owner := s.owner
	s.access.Unlock()
	if owner != nil {
		return owner.Close()
	}
	return s.conn.Close()
}
