package netstack

import (
	"context"
	"encoding/binary"
	"io"
	"net"
	"strings"
	"sync"
	"time"

	E "github.com/sagernet/sing/common/exceptions"

	mDNS "github.com/miekg/dns"
)

const dnsTimeout = 10 * time.Second

// The mDNSResponder daemon speaks an undocumented binary protocol over an AF_UNIX SOCK_STREAM socket, framed
// as in the client stub of Apple's open-source mDNSResponder (mDNSShared/dnssd_ipc.h, dnssd_clientstub.c and
// uds_daemon.c), with every multi-byte field big-endian. A stream opened with connection_request is a shared
// connection (DNSServiceCreateConnection): each request carries a client_context in header bytes 16-24, which
// the daemon echoes back in every reply. With IPC_FLAGS_NOERRSD the daemon expects no SCM_RIGHTS error-return
// socket: a request error arrives as an async_error_op reply, and success is not acknowledged. cancel_request
// with the same client_context and no payload cancels a query.
const (
	mdnsResponderSocketPath        = "/var/run/mDNSResponder"
	mdnsResponderVersion           = 1
	mdnsResponderHeaderLength      = 28
	mdnsResponderConnectionRequest = 1  // connection_request
	mdnsResponderQueryRequest      = 8  // query_request
	mdnsResponderCancelRequest     = 63 // cancel_request
	mdnsResponderQueryReply        = 68 // query_reply_op
	mdnsResponderAsyncErrorReply   = 73 // async_error_op

	mdnsResponderFlagMoreComing          = 0x1
	mdnsResponderFlagAdd                 = 0x2
	mdnsResponderFlagReturnIntermediates = 0x1000
	mdnsResponderFlagShareConnection     = 0x4000
	mdnsResponderFlagTimeout             = 0x10000

	mdnsResponderIPCFlagNoErrorSocket = 0x4 // IPC_FLAGS_NOERRSD

	mdnsResponderErrNoError      = 0
	mdnsResponderErrNoSuchName   = -65538
	mdnsResponderErrNoSuchRecord = -65554
	mdnsResponderErrTimeout      = -65568

	mdnsResponderMaxCNAMEReferrals = 10
)

type dnsResolver struct {
	ctx         context.Context
	connAccess  sync.Mutex
	conn        net.Conn
	queryAccess sync.Mutex
	queryID     uint64
	queries     map[uint64]*dnsPendingQuery
}

type dnsPendingQuery struct {
	conn           net.Conn
	question       mDNS.Question
	answers        []mDNS.RR
	cnameReferrals int
	ready          bool
	rcode          int
	err            error
	done           chan struct{}
}

func (s *Stack) exchangeDNS(ctx context.Context, query *mDNS.Msg) *mDNS.Msg {
	response := new(mDNS.Msg)
	if len(query.Question) != 1 {
		return response.SetRcodeFormatError(query)
	}
	ctx, cancel := context.WithTimeout(ctx, dnsTimeout)
	defer cancel()
	answers, rcode, err := s.dns.lookup(ctx, query.Question[0])
	if err != nil {
		rcode = mDNS.RcodeServerFailure
	}
	response.SetRcode(query, rcode)
	response.RecursionAvailable = true
	response.Answer = answers
	opt := query.IsEdns0()
	if opt != nil {
		response.SetEdns0(opt.UDPSize(), false)
	}
	return response
}

func (s *Stack) serveDNSStream(ctx context.Context, conn net.Conn) error {
	var length [2]byte
	for {
		_, err := io.ReadFull(conn, length[:])
		if err != nil {
			return err
		}
		packet := make([]byte, binary.BigEndian.Uint16(length[:]))
		_, err = io.ReadFull(conn, packet)
		if err != nil {
			return err
		}
		var query mDNS.Msg
		err = query.Unpack(packet)
		if err != nil {
			return err
		}
		response := s.exchangeDNS(ctx, &query)
		response.Truncate(mDNS.MaxMsgSize)
		packet, err = response.Pack()
		if err != nil {
			return err
		}
		_, err = conn.Write(append(binary.BigEndian.AppendUint16(make([]byte, 0, 2+len(packet)), uint16(len(packet))), packet...))
		if err != nil {
			return err
		}
	}
}

func (r *dnsResolver) lookup(ctx context.Context, question mDNS.Question) ([]mDNS.RR, int, error) {
	for firstAttempt := true; ; firstAttempt = false {
		conn, created, err := r.connection(ctx)
		if err != nil {
			return nil, 0, err
		}
		pending := &dnsPendingQuery{conn: conn, question: question, done: make(chan struct{})}
		r.queryAccess.Lock()
		r.queryID++
		queryID := r.queryID
		r.queries[queryID] = pending
		r.queryAccess.Unlock()
		_, err = conn.Write(buildQueryRequest(queryID, question))
		if err != nil {
			r.invalidate(conn, err)
			if !created && firstAttempt {
				continue
			}
			return nil, 0, E.Cause(err, "write mDNSResponder query")
		}
		select {
		case <-pending.done:
			r.cancel(conn, queryID)
			return pending.answers, pending.rcode, pending.err
		case <-ctx.Done():
			r.cancel(conn, queryID)
			return nil, 0, ctx.Err()
		}
	}
}

func (r *dnsResolver) connection(ctx context.Context) (net.Conn, bool, error) {
	r.connAccess.Lock()
	defer r.connAccess.Unlock()
	if r.conn != nil {
		return r.conn, false, nil
	}
	var dialer net.Dialer
	conn, err := dialer.DialContext(ctx, "unix", mdnsResponderSocketPath)
	if err != nil {
		return nil, false, E.Cause(err, "connect mDNSResponder")
	}
	stopCancel := context.AfterFunc(ctx, func() {
		conn.Close()
	})
	err = writeConnectionRequest(conn)
	if !stopCancel() {
		err = ctx.Err()
	}
	if err != nil {
		conn.Close()
		return nil, false, err
	}
	r.conn = conn
	go r.loopReceive(conn)
	return conn, true, nil
}

func writeConnectionRequest(conn net.Conn) error {
	_, err := conn.Write(appendResponderHeader(make([]byte, 0, mdnsResponderHeaderLength), mdnsResponderConnectionRequest, 0, 0, 0))
	if err != nil {
		return E.Cause(err, "write mDNSResponder connection request")
	}
	var status [4]byte
	_, err = io.ReadFull(conn, status[:])
	if err != nil {
		return E.Cause(err, "read mDNSResponder connection status")
	}
	statusCode := int32(binary.BigEndian.Uint32(status[:]))
	if statusCode != mdnsResponderErrNoError {
		return E.New("mDNSResponder connection request failed: error ", statusCode)
	}
	return nil
}

func (r *dnsResolver) cancel(conn net.Conn, queryID uint64) {
	r.queryAccess.Lock()
	delete(r.queries, queryID)
	r.queryAccess.Unlock()
	_, err := conn.Write(appendResponderHeader(make([]byte, 0, mdnsResponderHeaderLength), mdnsResponderCancelRequest, 0, queryID, 0))
	if err != nil {
		r.invalidate(conn, err)
	}
}

func (r *dnsResolver) invalidate(conn net.Conn, err error) {
	r.connAccess.Lock()
	if r.conn == conn {
		r.conn = nil
	}
	r.connAccess.Unlock()
	conn.Close()
	r.queryAccess.Lock()
	defer r.queryAccess.Unlock()
	for queryID, pending := range r.queries {
		if pending.conn == conn {
			delete(r.queries, queryID)
			pending.err = err
			close(pending.done)
		}
	}
}

func (r *dnsResolver) loopReceive(conn net.Conn) {
	stopCancel := context.AfterFunc(r.ctx, func() {
		conn.Close()
	})
	defer stopCancel()
	for {
		operation, queryID, data, err := readResponderReply(conn)
		if err == nil {
			err = r.handleReply(operation, queryID, data)
		}
		if err != nil {
			r.invalidate(conn, err)
			return
		}
	}
}

func (r *dnsResolver) handleReply(operation uint32, queryID uint64, data []byte) error {
	r.queryAccess.Lock()
	defer r.queryAccess.Unlock()
	pending, loaded := r.queries[queryID]
	var flags uint32
	switch operation {
	case mdnsResponderQueryReply:
		reply, err := parseResponderReply(data)
		if err != nil {
			return err
		}
		flags = reply.flags
		if !loaded {
			break
		}
		if reply.errorCode != mdnsResponderErrNoError {
			delete(r.queries, queryID)
			if len(pending.answers) > 0 {
				pending.complete(mdnsResponderErrNoError)
			} else {
				pending.complete(reply.errorCode)
			}
			break
		}
		if reply.flags&mdnsResponderFlagAdd != 0 && len(reply.rdata) > 0 {
			record, buildErr := buildResourceRecord(reply)
			if buildErr == nil {
				pending.addAnswer(record)
			}
		}
	case mdnsResponderAsyncErrorReply:
		reader := replyReader{data: data}
		flags = reader.uint32()
		reader.uint32() // interfaceIndex
		errorCode := int32(reader.uint32())
		if reader.err != nil {
			return reader.err
		}
		if loaded {
			delete(r.queries, queryID)
			pending.complete(errorCode)
		}
	default:
		return nil
	}
	if flags&mdnsResponderFlagMoreComing == 0 {
		for readyID, ready := range r.queries {
			if ready.ready {
				delete(r.queries, readyID)
				ready.complete(mdnsResponderErrNoError)
			}
		}
	}
	return nil
}

// The daemon follows a CNAME answer by restarting the query at its target, except a CNAME that points to its
// own name or arrives after mdnsResponderMaxCNAMEReferrals referrals (AnswerQuestionByFollowingCNAME in
// mDNSCore/mDNS.c), which ends the chain without a further reply.
func (p *dnsPendingQuery) addAnswer(record mDNS.RR) {
	p.answers = append(p.answers, record)
	header := record.Header()
	switch {
	case header.Rrtype == p.question.Qtype:
		p.ready = true
	case header.Rrtype == mDNS.TypeCNAME:
		p.cnameReferrals++
		if p.cnameReferrals > mdnsResponderMaxCNAMEReferrals || strings.EqualFold(header.Name, record.(*mDNS.CNAME).Target) {
			p.ready = true
		}
	case p.question.Qtype == mDNS.TypeANY:
		p.ready = true
	}
}

// The daemon's NoSuchRecord conflates NXDOMAIN and NODATA.
func (p *dnsPendingQuery) complete(code int32) {
	switch code {
	case mdnsResponderErrNoError, mdnsResponderErrNoSuchRecord:
		p.rcode = mDNS.RcodeSuccess
	case mdnsResponderErrNoSuchName:
		p.rcode = mDNS.RcodeNameError
	case mdnsResponderErrTimeout:
		p.err = E.New("mDNSResponder query timeout")
	default:
		p.err = E.New("mDNSResponder query failed: error ", code)
	}
	close(p.done)
}

func appendResponderHeader(buffer []byte, operation uint32, dataLength int, clientContext uint64, ipcFlags uint32) []byte {
	buffer = binary.BigEndian.AppendUint32(buffer, mdnsResponderVersion)
	buffer = binary.BigEndian.AppendUint32(buffer, uint32(dataLength))
	buffer = binary.BigEndian.AppendUint32(buffer, ipcFlags)
	buffer = binary.BigEndian.AppendUint32(buffer, operation)
	buffer = binary.BigEndian.AppendUint64(buffer, clientContext)
	buffer = binary.BigEndian.AppendUint32(buffer, 0) // reg_index
	return buffer
}

func buildQueryRequest(queryID uint64, question mDNS.Question) []byte {
	payloadLength := 4 + 4 + len(question.Name) + 1 + 2 + 2
	message := make([]byte, 0, mdnsResponderHeaderLength+payloadLength)
	message = appendResponderHeader(message, mdnsResponderQueryRequest, payloadLength, queryID, mdnsResponderIPCFlagNoErrorSocket)
	message = binary.BigEndian.AppendUint32(message, mdnsResponderFlagShareConnection|mdnsResponderFlagReturnIntermediates|mdnsResponderFlagTimeout)
	message = binary.BigEndian.AppendUint32(message, 0) // interfaceIndex
	message = append(message, question.Name...)
	message = append(message, 0)
	message = binary.BigEndian.AppendUint16(message, question.Qtype)
	message = binary.BigEndian.AppendUint16(message, question.Qclass)
	return message
}

func readResponderReply(conn net.Conn) (operation uint32, clientContext uint64, data []byte, err error) {
	var header [mdnsResponderHeaderLength]byte
	_, err = io.ReadFull(conn, header[:])
	if err != nil {
		return
	}
	dataLength := binary.BigEndian.Uint32(header[4:8])
	operation = binary.BigEndian.Uint32(header[12:16])
	clientContext = binary.BigEndian.Uint64(header[16:24])
	data = make([]byte, dataLength)
	_, err = io.ReadFull(conn, data)
	return
}

type mdnsResponderReply struct {
	flags     uint32
	errorCode int32
	name      string
	rrtype    uint16
	rrclass   uint16
	ttl       uint32
	rdata     []byte
}

func parseResponderReply(data []byte) (mdnsResponderReply, error) {
	var reply mdnsResponderReply
	reader := replyReader{data: data}
	reply.flags = reader.uint32()
	reader.uint32() // interfaceIndex
	reply.errorCode = int32(reader.uint32())
	reply.name = reader.cString()
	reply.rrtype = reader.uint16()
	reply.rrclass = reader.uint16()
	rdlen := reader.uint16()
	reply.rdata = reader.bytes(int(rdlen))
	reply.ttl = reader.uint32()
	return reply, reader.err
}

func buildResourceRecord(reply mdnsResponderReply) (mDNS.RR, error) {
	nameBuffer := make([]byte, 256)
	offset, err := mDNS.PackDomainName(mDNS.Fqdn(reply.name), nameBuffer, 0, nil, false)
	if err != nil {
		return nil, err
	}
	record := make([]byte, 0, offset+10+len(reply.rdata))
	record = append(record, nameBuffer[:offset]...)
	record = binary.BigEndian.AppendUint16(record, reply.rrtype)
	record = binary.BigEndian.AppendUint16(record, reply.rrclass)
	record = binary.BigEndian.AppendUint32(record, reply.ttl)
	record = binary.BigEndian.AppendUint16(record, uint16(len(reply.rdata)))
	record = append(record, reply.rdata...)
	resourceRecord, _, err := mDNS.UnpackRR(record, 0)
	return resourceRecord, err
}

type replyReader struct {
	data   []byte
	offset int
	err    error
}

func (r *replyReader) uint32() uint32 {
	if r.err != nil || r.offset+4 > len(r.data) {
		r.fail()
		return 0
	}
	value := binary.BigEndian.Uint32(r.data[r.offset:])
	r.offset += 4
	return value
}

func (r *replyReader) uint16() uint16 {
	if r.err != nil || r.offset+2 > len(r.data) {
		r.fail()
		return 0
	}
	value := binary.BigEndian.Uint16(r.data[r.offset:])
	r.offset += 2
	return value
}

func (r *replyReader) cString() string {
	if r.err != nil {
		return ""
	}
	end := r.offset
	for end < len(r.data) && r.data[end] != 0 {
		end++
	}
	if end >= len(r.data) {
		r.fail()
		return ""
	}
	value := string(r.data[r.offset:end])
	r.offset = end + 1
	return value
}

func (r *replyReader) bytes(length int) []byte {
	if r.err != nil || r.offset+length > len(r.data) {
		r.fail()
		return nil
	}
	value := r.data[r.offset : r.offset+length]
	r.offset += length
	return value
}

func (r *replyReader) fail() {
	if r.err == nil {
		r.err = E.New("truncated mDNSResponder reply")
	}
}
