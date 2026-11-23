package httpclient_test

import (
	"crypto/x509"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/swipt-automation/SwiptermKit/internal/httpclient"
)

const defaultUserAgent = "SwiptermKit/1.0"

func newTLSServer(t *testing.T, handler http.Handler) (*httptest.Server, *httpclient.Client) {
	server := httptest.NewUnstartedServer(handler)
	server.EnableHTTP2 = true
	server.StartTLS()
	t.Cleanup(server.Close)
	pool := x509.NewCertPool()
	pool.AddCert(server.Certificate())
	return server, httpclient.NewClient(pool)
}

func TestClientUserAgent(t *testing.T) {
	agents := make(chan string, 1)
	server, client := newTLSServer(t, http.HandlerFunc(func(writer http.ResponseWriter, request *http.Request) {
		agents <- request.UserAgent()
	}))
	for _, test := range []struct {
		name     string
		header   string
		expected string
	}{
		{name: "default", expected: defaultUserAgent},
		{name: "explicit", header: "Custom/2.0", expected: "Custom/2.0"},
	} {
		t.Run(test.name, func(t *testing.T) {
			request, err := http.NewRequest(http.MethodGet, server.URL, nil)
			if err != nil {
				t.Fatal(err)
			}
			if test.header != "" {
				request.Header.Set("User-Agent", test.header)
			}
			response, err := client.Do(request)
			if err != nil {
				t.Fatal(err)
			}
			response.Body.Close()
			agent := <-agents
			if agent != test.expected {
				t.Fatalf("User-Agent = %q, want %q", agent, test.expected)
			}
		})
	}
}
