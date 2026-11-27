package netstack

import (
	"context"
	"net"
	"net/netip"
	"sync"
	"time"

	"github.com/sagernet/sing-tun"
	"github.com/sagernet/sing-tun/ping"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	"github.com/swipt-automation/SwiptermKit/internal/vmm"
)

const (
	udpTimeout     = 5 * time.Minute
	icmpFlowMax    = 1024
	transmitFrames = 256
	transmitArena  = 8 * vmm.MaxFrame
)

var (
	inet4Prefix  = netip.MustParsePrefix("100.115.92.0/30")
	inet6Prefix  = netip.MustParsePrefix("fd53:7769:7074::/126")
	inet4Gateway = netip.MustParseAddr("100.115.92.1")
	inet6Gateway = netip.MustParseAddr("fd53:7769:7074::1")
	inet4Guest   = netip.MustParseAddr("100.115.92.2")
	inet6Guest   = netip.MustParseAddr("fd53:7769:7074::2")
	gatewayMAC   = net.HardwareAddr{0x52, 0x54, 0x00, 0x53, 0x57, 0x02}
)

type Stack struct {
	ctx            context.Context
	cancel         context.CancelFunc
	network        *vmm.Network
	tun            *tun.MemoryTun
	stack          *tun.Go
	icmp           *ping.Port
	dns            *dnsResolver
	mirror         *portMirror
	outboundAccess sync.Mutex
	closed         bool
	loops          sync.WaitGroup
}

func New() (*Stack, error) {
	network, err := vmm.NewNetwork()
	if err != nil {
		return nil, err
	}
	ctx, cancel := context.WithCancel(context.Background())
	s := &Stack{
		ctx:     ctx,
		cancel:  cancel,
		network: network,
		icmp:    ping.NewPort(ctx, logger.NOP(), nil, udpTimeout, icmpFlowMax),
		dns:     &dnsResolver{ctx: ctx, queries: make(map[uint64]*dnsPendingQuery)},
	}
	s.tun = tun.NewMemoryTun(tun.MemoryTunOptions{
		MTU:             vmm.MTU,
		Outbound:        s.writeOutbound,
		Ethernet:        true,
		VirtioNetHeader: true,
		GatewayMAC:      gatewayMAC,
		Inet6Prefix:     []netip.Prefix{inet6Prefix},
	})
	s.stack, err = tun.NewGo(tun.StackOptions{
		Context: ctx,
		Tun:     s.tun,
		TunOptions: tun.Options{
			MTU:          vmm.MTU,
			Inet4Address: []netip.Prefix{netip.PrefixFrom(inet4Gateway, inet4Prefix.Bits())},
			Inet6Address: []netip.Prefix{netip.PrefixFrom(inet6Gateway, inet6Prefix.Bits())},
		},
		UDPTimeout:    udpTimeout,
		ICMPTimeout:   udpTimeout,
		UDPMapping:    tun.NATMappingEndpointIndependent,
		UDPFiltering:  tun.NATFilteringEndpointIndependent,
		Handler:       s,
		Logger:        logger.NOP(),
		BufferProfile: tun.BufferProfileFull,
	})
	if err == nil {
		err = s.stack.Start()
	}
	if err != nil {
		s.tun.Close()
		s.icmp.Close()
		network.Close()
		cancel()
		return nil, E.Cause(err, "start network stack")
	}
	s.mirror = newPortMirror(s)
	s.loops.Go(s.loopTransmit)
	s.loops.Go(s.loopEvents)
	return s, nil
}

func (s *Stack) Handle() uintptr {
	return s.network.Handle()
}

func (s *Stack) ResetNetwork() {
	s.stack.ResetNetwork()
}

func (s *Stack) Close() error {
	s.cancel()
	err := s.stack.Close()
	s.tun.Close()
	s.network.CloseWakes()
	s.loops.Wait()
	s.mirror.close()
	s.outboundAccess.Lock()
	s.closed = true
	s.outboundAccess.Unlock()
	err = E.Errors(err, s.icmp.Close())
	s.network.Release()
	return err
}

func (s *Stack) loopTransmit() {
	arena := make([]byte, transmitArena)
	lengths := make([]uint32, transmitFrames)
	packets := make([][]byte, 0, transmitFrames)
	for s.network.Wait(vmm.WakeTX) == nil {
		for {
			count := s.network.PopTX(arena, lengths)
			if count == 0 {
				break
			}
			packets = packets[:0]
			offset := 0
			for _, length := range lengths[:count] {
				packet := arena[offset : offset+int(length)]
				offset += int(length)
				if !s.handleDHCP4(packet) {
					packets = append(packets, packet)
				}
			}
			if s.tun.WaitWritePackets(packets) != nil {
				return
			}
		}
	}
}

func (s *Stack) loopEvents() {
	var listeners []vmm.Listener
	for {
		var reset bool
		listeners, reset = s.network.TakeListeners(listeners[:0])
		features := s.network.Features()
		s.tun.SetOffload(features&vmm.FeatureGuestChecksum != 0,
			features&vmm.FeatureGuestTSO4 != 0 && features&vmm.FeatureGuestTSO6 != 0,
			features&vmm.FeatureGuestUSO4 != 0 && features&vmm.FeatureGuestUSO6 != 0)
		s.mirror.update(listeners, reset)
		if s.network.Wait(vmm.WakeEvent) != nil {
			return
		}
	}
}

func (s *Stack) writeOutbound(packets []*buf.Buffer) {
	s.outboundAccess.Lock()
	defer s.outboundAccess.Unlock()
	defer buf.ReleaseMulti(packets)
	if s.closed {
		return
	}
	for index, packet := range packets {
		for !s.network.PushRX(packet.Bytes(), index == len(packets)-1) {
			if s.network.Wait(vmm.WakeRX) != nil {
				return
			}
		}
	}
}
