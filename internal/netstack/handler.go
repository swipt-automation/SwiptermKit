package netstack

import (
	"bytes"
	"context"
	"net"
	"net/netip"

	"github.com/sagernet/sing-tun"
	"github.com/sagernet/sing-tun/gtcpip/header"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

const dnsPort = 53

func (s *Stack) JudgeFlow(network uint8, source netip.AddrPort, destination netip.AddrPort, firstPacket []byte) tun.FlowVerdict {
	address := destination.Addr()
	virtual := inet4Prefix.Contains(address) || inet6Prefix.Contains(address)
	switch network {
	case uint8(header.ICMPv4ProtocolNumber), uint8(header.ICMPv6ProtocolNumber):
		if virtual {
			return tun.FlowVerdict{Action: tun.ActionAccept}
		}
		return tun.FlowVerdict{Action: tun.ActionFlow, Port: s.icmp}
	}
	if !virtual {
		return tun.FlowVerdict{Action: tun.ActionAccept}
	}
	if (address == inet4DNS || address == inet6DNS) && destination.Port() == dnsPort {
		if network == uint8(header.UDPProtocolNumber) {
			return tun.FlowVerdict{Action: tun.ActionHijackDNS}
		}
		return tun.FlowVerdict{Action: tun.ActionAccept}
	}
	return tun.FlowVerdict{Action: tun.ActionReject}
}

func (s *Stack) NewDNSPacket(payload []byte, source M.Socksaddr, destination M.Socksaddr, writer N.PacketWriter) {
	query := bytes.Clone(payload)
	go func() {
		response, err := s.dns.exchange(s.ctx, query)
		if err != nil {
			s.logger.DebugContext(s.ctx, "dns: exchange for ", source, ": ", err)
			return
		}
		err = writer.WritePacket(response, destination)
		if err != nil {
			s.logger.DebugContext(s.ctx, "dns: write response to ", source, ": ", err)
		}
	}()
}

func (s *Stack) NewConnectionEx(ctx context.Context, conn net.Conn, source M.Socksaddr, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	if destination.Addr == inet4DNS || destination.Addr == inet6DNS {
		server, loaded := s.dns.server()
		if !loaded {
			N.CloseOnHandshakeFailure(conn, onClose, errNoDNSServer)
			return
		}
		destination = M.SocksaddrFromNetIP(server)
	}
	s.connections.NewConnection(ctx, conn, destination, onClose)
}

func (s *Stack) NewPacketConnectionEx(ctx context.Context, conn N.PacketConn, source M.Socksaddr, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	s.connections.NewPacketConnection(ctx, conn, destination, onClose)
}
