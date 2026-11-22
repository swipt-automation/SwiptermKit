package netstack

import (
	"context"
	"encoding/binary"
	"io"
	"net"
	"net/netip"
	"sync"
	"time"

	"github.com/sagernet/sing-tun/dnsinfo"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
)

const (
	dnsCacheTTL        = 5 * time.Second
	dnsExchangeTimeout = 4 * time.Second
	dnsHeaderLength    = 12
	dnsFlagTruncated   = 0x02
)

var errNoDNSServer = E.New("dns: no system DNS server")

type dnsForwarder struct {
	access    sync.Mutex
	expiresAt time.Time
	servers   []netip.AddrPort
}

func (f *dnsForwarder) reset() {
	f.access.Lock()
	f.expiresAt = time.Time{}
	f.servers = nil
	f.access.Unlock()
}

func (f *dnsForwarder) load() []netip.AddrPort {
	f.access.Lock()
	defer f.access.Unlock()
	now := time.Now()
	if len(f.servers) == 0 || !now.Before(f.expiresAt) {
		configuration := dnsinfo.Copy()
		if configuration != nil {
			f.servers = configuration.Select(0).Servers
		} else {
			f.servers = nil
		}
		if len(f.servers) > 0 {
			f.expiresAt = now.Add(dnsCacheTTL)
		} else {
			f.expiresAt = time.Time{}
		}
	}
	return f.servers
}

func (f *dnsForwarder) server() (netip.AddrPort, bool) {
	servers := f.load()
	if len(servers) == 0 {
		return netip.AddrPort{}, false
	}
	return servers[0], true
}

func (f *dnsForwarder) exchange(ctx context.Context, query []byte) (*buf.Buffer, error) {
	if len(query) < dnsHeaderLength {
		return nil, E.New("dns: short query")
	}
	servers := f.load()
	if len(servers) == 0 {
		return nil, errNoDNSServer
	}
	var err error
	for _, server := range servers {
		var response *buf.Buffer
		response, err = exchangeUDP(ctx, server, query)
		if err == nil && response.Byte(2)&dnsFlagTruncated != 0 {
			response.Release()
			response, err = exchangeTCP(ctx, server, query)
		}
		if err == nil {
			return response, nil
		}
	}
	return nil, err
}

func exchangeUDP(ctx context.Context, server netip.AddrPort, query []byte) (*buf.Buffer, error) {
	var dialer net.Dialer
	conn, err := dialer.DialContext(ctx, "udp", server.String())
	if err != nil {
		return nil, err
	}
	defer conn.Close()
	conn.SetDeadline(time.Now().Add(dnsExchangeTimeout))
	_, err = conn.Write(query)
	if err != nil {
		return nil, err
	}
	response := buf.NewPacket()
	for {
		response.Reset()
		_, err = response.ReadOnceFrom(conn)
		if err != nil {
			response.Release()
			return nil, err
		}
		if response.Len() >= dnsHeaderLength && binary.BigEndian.Uint16(response.To(2)) == binary.BigEndian.Uint16(query) {
			return response, nil
		}
	}
}

func exchangeTCP(ctx context.Context, server netip.AddrPort, query []byte) (*buf.Buffer, error) {
	ctx, cancel := context.WithTimeout(ctx, dnsExchangeTimeout)
	defer cancel()
	var dialer net.Dialer
	conn, err := dialer.DialContext(ctx, "tcp", server.String())
	if err != nil {
		return nil, err
	}
	defer conn.Close()
	deadline, _ := ctx.Deadline()
	conn.SetDeadline(deadline)
	request := make([]byte, 2+len(query))
	binary.BigEndian.PutUint16(request, uint16(len(query)))
	copy(request[2:], query)
	_, err = conn.Write(request)
	if err != nil {
		return nil, err
	}
	var length [2]byte
	_, err = io.ReadFull(conn, length[:])
	if err != nil {
		return nil, err
	}
	response := buf.NewSize(int(binary.BigEndian.Uint16(length[:])))
	_, err = response.ReadFullFrom(conn, response.FreeLen())
	if err != nil {
		response.Release()
		return nil, err
	}
	return response, nil
}
