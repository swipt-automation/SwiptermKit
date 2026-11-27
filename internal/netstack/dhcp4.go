package netstack

import (
	"encoding/binary"
	"net/netip"

	"github.com/sagernet/sing-tun/gtcpip"
	"github.com/sagernet/sing-tun/gtcpip/checksum"
	"github.com/sagernet/sing-tun/gtcpip/header"
	"github.com/sagernet/sing/common/buf"
	"github.com/swipt-automation/SwiptermKit/internal/vmm"
)

const (
	dhcpServerPort         = 67
	dhcpClientPort         = 68
	dhcpLeaseTime          = 24 * 60 * 60
	dhcpHeaderLength       = 240
	dhcpMinimumLength      = 300
	dhcpFlagBroadcast      = 0x8000
	dhcpFrameHeaderLength  = vmm.HeaderSize + header.EthernetMinimumSize + header.IPv4MinimumSize + header.UDPMinimumSize
	dhcpTTL                = 64
	virtioNumBuffersOffset = 10

	dhcpBootRequest      = 1
	dhcpBootReply        = 2
	dhcpHardwareEthernet = 1

	dhcpDiscover = 1
	dhcpOffer    = 2
	dhcpRequest  = 3
	dhcpAck      = 5
	dhcpNak      = 6
	dhcpInform   = 8

	dhcpOptionPad              = 0
	dhcpOptionSubnetMask       = 1
	dhcpOptionRouter           = 3
	dhcpOptionDNS              = 6
	dhcpOptionMTU              = 26
	dhcpOptionRequestedAddress = 50
	dhcpOptionLeaseTime        = 51
	dhcpOptionMessageType      = 53
	dhcpOptionServerIdentifier = 54
	dhcpOptionEnd              = 255
)

var (
	dhcpMagicCookie = [4]byte{99, 130, 83, 99}
	broadcastMAC    = [header.EthernetAddressSize]byte{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
	inet4Broadcast  = netip.AddrFrom4([4]byte{0xff, 0xff, 0xff, 0xff})
)

func (s *Stack) handleDHCP4(frame []byte) bool {
	ethernet := header.Ethernet(frame[vmm.HeaderSize:])
	if len(ethernet) < header.EthernetMinimumSize || ethernet.Type() != header.IPv4ProtocolNumber {
		return false
	}
	ip := header.IPv4(ethernet[header.EthernetMinimumSize:])
	if !ip.IsValid(len(ip)) || ip.TransportProtocol() != header.UDPProtocolNumber {
		return false
	}
	udp := header.UDP(ip.Payload())
	if len(udp) < header.UDPMinimumSize || udp.SourcePort() != dhcpClientPort || udp.DestinationPort() != dhcpServerPort {
		return false
	}
	reply := dhcp4Reply(udp.Payload())
	if reply != nil {
		s.writeOutbound([]*buf.Buffer{buf.As(reply)})
	}
	return true
}

func dhcp4Reply(request []byte) []byte {
	if len(request) < dhcpHeaderLength || request[0] != dhcpBootRequest || request[1] != dhcpHardwareEthernet ||
		request[2] != header.EthernetAddressSize || [4]byte(request[236:240]) != dhcpMagicCookie {
		return nil
	}
	var (
		messageType byte
		requested   netip.Addr
	)
	for options := request[dhcpHeaderLength:]; len(options) > 0 && options[0] != dhcpOptionEnd; {
		if options[0] == dhcpOptionPad {
			options = options[1:]
			continue
		}
		if len(options) < 2 || len(options) < 2+int(options[1]) {
			break
		}
		value := options[2 : 2+options[1]]
		switch {
		case options[0] == dhcpOptionMessageType && len(value) == 1:
			messageType = value[0]
		case options[0] == dhcpOptionRequestedAddress && len(value) == 4:
			requested = netip.AddrFrom4([4]byte(value))
		}
		options = options[2+len(value):]
	}
	ciaddr := netip.AddrFrom4([4]byte(request[12:16]))
	yiaddr := inet4Guest
	var reply byte
	switch messageType {
	case dhcpDiscover:
		reply = dhcpOffer
	case dhcpRequest:
		reply = dhcpAck
		if !requested.IsValid() && !ciaddr.IsUnspecified() {
			requested = ciaddr
		}
		if requested.IsValid() && requested != inet4Guest {
			reply = dhcpNak
		}
	case dhcpInform:
		reply = dhcpAck
		yiaddr = netip.IPv4Unspecified()
	default:
		return nil
	}

	destination := ciaddr
	if destination.IsUnspecified() && binary.BigEndian.Uint16(request[10:])&dhcpFlagBroadcast == 0 {
		destination = yiaddr
	}
	destinationMAC := request[28 : 28+header.EthernetAddressSize]
	if reply == dhcpNak || destination.IsUnspecified() {
		destination = inet4Broadcast
		destinationMAC = broadcastMAC[:]
	}

	frame := make([]byte, dhcpFrameHeaderLength+dhcpHeaderLength, dhcpFrameHeaderLength+dhcpMinimumLength)
	message := frame[dhcpFrameHeaderLength:]
	message[0] = dhcpBootReply
	message[1] = dhcpHardwareEthernet
	message[2] = header.EthernetAddressSize
	copy(message[4:8], request[4:8])
	copy(message[10:12], request[10:12])
	if reply != dhcpNak {
		if reply == dhcpAck {
			copy(message[12:16], request[12:16])
		}
		copy(message[16:20], yiaddr.AsSlice())
		copy(message[20:24], inet4Gateway.AsSlice())
	}
	copy(message[24:44], request[24:44])
	copy(message[236:240], dhcpMagicCookie[:])
	frame = append(frame, dhcpOptionMessageType, 1, reply)
	frame = append(append(frame, dhcpOptionServerIdentifier, 4), inet4Gateway.AsSlice()...)
	if reply != dhcpNak {
		if messageType != dhcpInform {
			frame = binary.BigEndian.AppendUint32(append(frame, dhcpOptionLeaseTime, 4), dhcpLeaseTime)
		}
		frame = binary.BigEndian.AppendUint32(append(frame, dhcpOptionSubnetMask, 4), ^uint32(0)<<(32-inet4Prefix.Bits()))
		frame = append(append(frame, dhcpOptionRouter, 4), inet4Gateway.AsSlice()...)
		frame = append(append(frame, dhcpOptionDNS, 4), inet4Gateway.AsSlice()...)
		frame = binary.BigEndian.AppendUint16(append(frame, dhcpOptionMTU, 2), vmm.MTU)
	}
	frame = append(frame, dhcpOptionEnd)
	frame = frame[:cap(frame)]

	binary.LittleEndian.PutUint16(frame[virtioNumBuffersOffset:], 1)
	header.Ethernet(frame[vmm.HeaderSize:]).Encode(&header.EthernetFields{
		DstAddr: tcpip.LinkAddress(destinationMAC),
		SrcAddr: tcpip.LinkAddress(gatewayMAC),
		Type:    header.IPv4ProtocolNumber,
	})
	ip := header.IPv4(frame[vmm.HeaderSize+header.EthernetMinimumSize:])
	ip.Encode(&header.IPv4Fields{
		TotalLength: uint16(len(ip)),
		TTL:         dhcpTTL,
		Protocol:    uint8(header.UDPProtocolNumber),
		SrcAddr:     inet4Gateway,
		DstAddr:     destination,
	})
	ip.SetChecksum(^ip.CalculateChecksum())
	udp := header.UDP(ip[header.IPv4MinimumSize:])
	udp.Encode(&header.UDPFields{
		SrcPort: dhcpServerPort,
		DstPort: dhcpClientPort,
		Length:  uint16(len(udp)),
	})
	sum := header.PseudoHeaderChecksum(header.UDPProtocolNumber, inet4Gateway.AsSlice(), destination.AsSlice(), uint16(len(udp)))
	sum = ^udp.CalculateChecksum(checksum.Checksum(udp.Payload(), sum))
	if sum == 0 {
		sum = 0xffff
	}
	udp.SetChecksum(sum)
	return frame
}
