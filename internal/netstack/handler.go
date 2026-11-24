package netstack

import (
	"context"
	"net"
	"net/netip"

	"github.com/sagernet/sing-tun"
	"github.com/sagernet/sing-tun/gtcpip/header"
	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"

	mDNS "github.com/miekg/dns"
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
	var query mDNS.Msg
	err := query.Unpack(payload)
	if err != nil {
		s.logger.DebugContext(s.ctx, "dns: parse query from ", source, ": ", err)
		return
	}
	go func() {
		response := s.exchangeDNS(s.ctx, &query)
		maxLength := mDNS.MinMsgSize
		opt := query.IsEdns0()
		if opt != nil {
			maxLength = max(maxLength, int(opt.UDPSize()))
		}
		response.Truncate(maxLength)
		packet, err := response.Pack()
		if err == nil {
			err = writer.WritePacket(buf.As(packet), destination)
		}
		if err != nil {
			s.logger.DebugContext(s.ctx, "dns: write response to ", source, ": ", err)
		}
	}()
}

func (s *Stack) NewConnectionEx(ctx context.Context, conn net.Conn, source M.Socksaddr, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	if destination.Addr == inet4DNS || destination.Addr == inet6DNS {
		s.connections.closeHandler(ctx, conn, onClose)(s.serveDNSStream(ctx, conn))
		return
	}
	s.connections.NewConnection(ctx, conn, destination, onClose)
}

func (s *Stack) NewPacketConnectionEx(ctx context.Context, conn N.PacketConn, source M.Socksaddr, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	s.connections.NewPacketConnection(ctx, conn, destination, onClose)
}
