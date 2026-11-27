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
	if address == inet4Gateway || address == inet6Gateway {
		if destination.Port() == dnsPort {
			if network == uint8(header.UDPProtocolNumber) {
				return tun.FlowVerdict{Action: tun.ActionHijackDNS}
			}
			return tun.FlowVerdict{Action: tun.ActionAccept}
		}
		if network == uint8(header.TCPProtocolNumber) && !s.mirror.hasHostListener(address.Is6(), destination.Port()) {
			return tun.FlowVerdict{Action: tun.ActionAccept}
		}
	}
	return tun.FlowVerdict{Action: tun.ActionReject}
}

func (s *Stack) NewDNSPacket(payload []byte, source M.Socksaddr, destination M.Socksaddr, writer N.PacketWriter) {
	var query mDNS.Msg
	if query.Unpack(payload) != nil {
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
			writer.WritePacket(buf.As(packet), destination)
		}
	}()
}

func (s *Stack) NewConnectionEx(ctx context.Context, conn net.Conn, source M.Socksaddr, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	switch destination.Addr {
	case inet4Gateway:
		if destination.Port == dnsPort {
			closeHandler(conn, onClose)(s.serveDNSStream(ctx, conn))
			return
		}
		destination.Addr = netip.AddrFrom4([4]byte{127, 0, 0, 1})
	case inet6Gateway:
		if destination.Port == dnsPort {
			closeHandler(conn, onClose)(s.serveDNSStream(ctx, conn))
			return
		}
		destination.Addr = netip.IPv6Loopback()
	}
	newConnection(ctx, conn, destination, onClose)
}

func (s *Stack) NewPacketConnectionEx(ctx context.Context, conn N.PacketConn, source M.Socksaddr, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	newPacketConnection(ctx, conn, destination, onClose)
}
