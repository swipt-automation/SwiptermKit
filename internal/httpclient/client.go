package httpclient

import (
	"crypto/tls"
	"crypto/x509"
	"net"
	"net/http"
	"time"

	E "github.com/sagernet/sing/common/exceptions"
	F "github.com/sagernet/sing/common/format"
)

const (
	dialTimeout           = 15 * time.Second
	tlsHandshakeTimeout   = 15 * time.Second
	responseHeaderTimeout = 30 * time.Second
	idleConnTimeout       = 90 * time.Second
	redirectLimit         = 10
	userAgent             = "SwiptermKit/1.0"
)

var ErrEmptyBody = E.New("http: empty response body")

type StatusError struct {
	StatusCode int
}

func (e *StatusError) Error() string {
	return F.ToString("http: unexpected status: ", e.StatusCode, " ", http.StatusText(e.StatusCode))
}

type ContentLengthError struct {
	Expected int64
	Actual   int64
}

func (e *ContentLengthError) Error() string {
	return F.ToString("http: Content-Length mismatch: expected ", e.Expected, ", got ", e.Actual)
}

type TransientError struct {
	Cause error
}

func (e *TransientError) Error() string {
	return e.Cause.Error()
}

func (e *TransientError) Unwrap() error {
	return e.Cause
}

type Client struct {
	client http.Client
}

func NewClient(rootCAs *x509.CertPool) *Client {
	transport := &http.Transport{
		Proxy:                 http.ProxyFromEnvironment,
		DialContext:           (&net.Dialer{Timeout: dialTimeout}).DialContext,
		ForceAttemptHTTP2:     true,
		TLSClientConfig:       &tls.Config{MinVersion: tls.VersionTLS12, RootCAs: rootCAs},
		TLSHandshakeTimeout:   tlsHandshakeTimeout,
		ResponseHeaderTimeout: responseHeaderTimeout,
		IdleConnTimeout:       idleConnTimeout,
	}
	return &Client{client: http.Client{Transport: transport, CheckRedirect: checkRedirect}}
}

func (c *Client) Do(request *http.Request) (*http.Response, error) {
	if request.Header.Get("User-Agent") == "" {
		request.Header.Set("User-Agent", userAgent)
	}
	return c.client.Do(request)
}

func checkRedirect(request *http.Request, via []*http.Request) error {
	if len(via) >= redirectLimit {
		return E.New("http: stopped after ", redirectLimit, " redirects")
	}
	return nil
}
