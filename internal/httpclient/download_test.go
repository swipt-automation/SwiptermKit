package httpclient_test

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"math/rand/v2"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/swipt-automation/SwiptermKit/internal/httpclient"
)

const (
	contentSize = 17<<20 + 12345
	mergeSize   = 32<<20 + 12345
	contentETag = `"swipterm"`
	workerCount = 4
	waitLimit   = 10 * time.Second
)

var proxyHandler atomic.Value

func TestMain(m *testing.M) {
	proxy := httptest.NewServer(http.HandlerFunc(func(writer http.ResponseWriter, request *http.Request) {
		proxyHandler.Load().(http.Handler).ServeHTTP(writer, request)
	}))
	os.Setenv("HTTP_PROXY", proxy.URL)
	os.Unsetenv("NO_PROXY")
	os.Unsetenv("no_proxy")
	code := m.Run()
	proxy.Close()
	os.Exit(code)
}

type record struct {
	host      string
	byteRange string
	ifRange   string
	userAgent string
	protocol  int
	status    int
}

type recorder struct {
	access  sync.Mutex
	records []record
}

func (r *recorder) wrap(handler func(http.ResponseWriter, *http.Request) int) http.Handler {
	return http.HandlerFunc(func(writer http.ResponseWriter, request *http.Request) {
		status := handler(writer, request)
		r.access.Lock()
		r.records = append(r.records, record{
			host:      request.Host,
			byteRange: request.Header.Get("Range"),
			ifRange:   request.Header.Get("If-Range"),
			userAgent: request.UserAgent(),
			protocol:  request.ProtoMajor,
			status:    status,
		})
		r.access.Unlock()
	})
}

func (r *recorder) snapshot() []record {
	r.access.Lock()
	defer r.access.Unlock()
	return append([]record(nil), r.records...)
}

type progressRecorder struct {
	access    sync.Mutex
	snapshots []httpclient.Progress
	verifying int
}

func (p *progressRecorder) Progress(progress httpclient.Progress) {
	p.access.Lock()
	p.snapshots = append(p.snapshots, progress)
	p.access.Unlock()
}

func (p *progressRecorder) Verifying() {
	p.access.Lock()
	p.verifying++
	p.access.Unlock()
}

func (p *progressRecorder) last() httpclient.Progress {
	p.access.Lock()
	defer p.access.Unlock()
	return p.snapshots[len(p.snapshots)-1]
}

type gauge struct {
	current atomic.Int32
	peak    atomic.Int32
}

func (g *gauge) enter() {
	current := g.current.Add(1)
	for {
		observed := g.peak.Load()
		if current <= observed || g.peak.CompareAndSwap(observed, current) {
			return
		}
	}
}

func (g *gauge) leave() {
	g.current.Add(-1)
}

func newContent(size int) ([]byte, string) {
	content := make([]byte, size)
	rand.NewChaCha8([32]byte{}).Read(content)
	digest := sha256.Sum256(content)
	return content, hex.EncodeToString(digest[:])
}

func serveContent(writer http.ResponseWriter, request *http.Request, content []byte) int {
	writer.Header().Set("ETag", contentETag)
	status := http.StatusOK
	if request.Header.Get("Range") != "" {
		status = http.StatusPartialContent
	}
	http.ServeContent(writer, request, "", time.Time{}, bytes.NewReader(content))
	return status
}

func serveMirrors(mirrors map[string]http.Handler) {
	proxyHandler.Store(http.HandlerFunc(func(writer http.ResponseWriter, request *http.Request) {
		mirrors[request.Host].ServeHTTP(writer, request)
	}))
}

func mirrorURLs(hosts ...string) []string {
	links := make([]string, 0, len(hosts))
	for _, host := range hosts {
		links = append(links, "http://"+host+"/archive")
	}
	return links
}

func download(t *testing.T, client *httpclient.Client, options httpclient.DownloadOptions) ([]byte, *progressRecorder) {
	options.Destination = filepath.Join(t.TempDir(), "archive")
	progress := &progressRecorder{}
	err := client.Download(context.Background(), options, progress)
	if err != nil {
		t.Fatal(err)
	}
	downloaded, err := os.ReadFile(options.Destination)
	if err != nil {
		t.Fatal(err)
	}
	return downloaded, progress
}

func checkSources(t *testing.T, progress httpclient.Progress, size int64) {
	var written int64
	for _, entry := range progress.Sources {
		written += entry.Written
	}
	if progress.Written != size || written != size || progress.Streams != 0 {
		t.Fatalf("final progress = %+v, want %d bytes from sources and no streams", progress, size)
	}
}

func checkUserAgents(t *testing.T, records []record) {
	for _, entry := range records {
		if entry.userAgent != defaultUserAgent {
			t.Fatalf("User-Agent = %q, want %q", entry.userAgent, defaultUserAgent)
		}
	}
}

func TestDownloadParallel(t *testing.T) {
	content, digest := newContent(contentSize)
	var concurrency gauge
	var arrivals atomic.Int32
	arrived := make(chan struct{})
	requests := &recorder{}
	server, client := newTLSServer(t, requests.wrap(func(writer http.ResponseWriter, request *http.Request) int {
		if request.Header.Get("If-Range") == "" {
			return serveContent(writer, request, content)
		}
		concurrency.enter()
		defer concurrency.leave()
		if arrivals.Add(1) == workerCount {
			close(arrived)
		}
		select {
		case <-arrived:
		case <-time.After(waitLimit):
		}
		return serveContent(writer, request, content)
	}))

	downloaded, progress := download(t, client, httpclient.DownloadOptions{
		URL:            server.URL + "/archive",
		ExpectedBytes:  contentSize,
		ExpectedSHA256: digest,
	})
	if !bytes.Equal(downloaded, content) {
		t.Fatal("downloaded content differs")
	}
	if concurrency.peak.Load() != workerCount {
		t.Fatalf("peak concurrent chunk requests = %d, want %d", concurrency.peak.Load(), workerCount)
	}
	records := requests.snapshot()
	checkUserAgents(t, records)
	chunks := 0
	for _, entry := range records {
		if entry.byteRange == "" {
			t.Fatal("unexpected single-stream request")
		}
		if entry.ifRange == "" {
			continue
		}
		chunks++
		if entry.ifRange != contentETag || entry.status != http.StatusPartialContent || entry.protocol != 2 {
			t.Fatalf("unexpected chunk request: %+v", entry)
		}
	}
	if chunks <= workerCount {
		t.Fatalf("chunk requests = %d, want more than %d", chunks, workerCount)
	}
	final := progress.last()
	checkSources(t, final, contentSize)
	if len(final.Sources) != 1 || final.Sources[0].Index != 0 {
		t.Fatalf("final sources = %+v, want the only URL", final.Sources)
	}
	progress.access.Lock()
	defer progress.access.Unlock()
	for index, snapshot := range progress.snapshots {
		if snapshot.Total != contentSize || (index > 0 && snapshot.Written < progress.snapshots[index-1].Written) {
			t.Fatalf("progress %d = %d/%d is not monotonic", index, snapshot.Written, snapshot.Total)
		}
	}
	if progress.verifying != 1 {
		t.Fatalf("Verifying called %d times, want 1", progress.verifying)
	}
}

func TestDownloadAcademicHostSingleStream(t *testing.T) {
	content, digest := newContent(contentSize)
	requests := &recorder{}
	proxyHandler.Store(requests.wrap(func(writer http.ResponseWriter, request *http.Request) int {
		return serveContent(writer, request, content)
	}))

	downloaded, _ := download(t, httpclient.NewClient(nil), httpclient.DownloadOptions{
		URL:            "http://mirrors.example.edu.cn/archive",
		ExpectedBytes:  contentSize,
		ExpectedSHA256: digest,
	})
	if !bytes.Equal(downloaded, content) {
		t.Fatal("downloaded content differs")
	}
	records := requests.snapshot()
	checkUserAgents(t, records)
	if len(records) != 1 || records[0].host != "mirrors.example.edu.cn" || records[0].byteRange != "" {
		t.Fatalf("requests = %+v, want one plain request to mirrors.example.edu.cn", records)
	}
}

func TestDownloadFallback(t *testing.T) {
	content, digest := newContent(contentSize)
	for _, test := range []struct {
		name    string
		handler func(http.ResponseWriter, *http.Request) int
	}{
		{
			name: "too many requests",
			handler: func(writer http.ResponseWriter, request *http.Request) int {
				if request.Header.Get("If-Range") != "" {
					writer.WriteHeader(http.StatusTooManyRequests)
					return http.StatusTooManyRequests
				}
				return serveContent(writer, request, content)
			},
		},
		{
			name: "no range support",
			handler: func(writer http.ResponseWriter, request *http.Request) int {
				writer.Header().Set("ETag", contentETag)
				writer.Write(content)
				return http.StatusOK
			},
		},
	} {
		t.Run(test.name, func(t *testing.T) {
			requests := &recorder{}
			server, client := newTLSServer(t, requests.wrap(test.handler))

			downloaded, _ := download(t, client, httpclient.DownloadOptions{
				URL:            server.URL + "/archive",
				ExpectedBytes:  contentSize,
				ExpectedSHA256: digest,
			})
			if !bytes.Equal(downloaded, content) {
				t.Fatal("downloaded content differs")
			}
			records := requests.snapshot()
			checkUserAgents(t, records)
			if len(records) > 2+workerCount {
				t.Fatalf("requests = %+v, want at most %d", records, 2+workerCount)
			}
			plain := 0
			for _, entry := range records {
				if entry.byteRange == "" && entry.status == http.StatusOK {
					plain++
				}
			}
			if plain != 1 {
				t.Fatalf("requests = %+v, want one plain single-stream request", records)
			}
		})
	}
}

func TestDownloadForbiddenWithoutRetry(t *testing.T) {
	requests := &recorder{}
	server, client := newTLSServer(t, requests.wrap(func(writer http.ResponseWriter, request *http.Request) int {
		writer.WriteHeader(http.StatusForbidden)
		return http.StatusForbidden
	}))

	destination := filepath.Join(t.TempDir(), "archive")
	err := client.Download(context.Background(), httpclient.DownloadOptions{
		URL:         server.URL + "/archive",
		Destination: destination,
	}, &progressRecorder{})
	if status, isStatus := errors.AsType[*httpclient.StatusError](err); !isStatus || status.StatusCode != http.StatusForbidden {
		t.Fatalf("err = %v, want status 403", err)
	}
	if records := requests.snapshot(); len(records) != 1 {
		t.Fatalf("requests = %+v, want one", records)
	}
	if _, err = os.Stat(destination); !errors.Is(err, os.ErrNotExist) {
		t.Fatalf("destination stat err = %v, want not exist", err)
	}
}

func TestDownloadMergesMirrors(t *testing.T) {
	content, digest := newContent(mergeSize)
	hosts := []string{"mirrors.example.edu.cn", "mirror-a.example", "mirror-b.example"}
	var overall gauge
	var arrivals atomic.Int32
	arrived := make(chan struct{})
	gauges := make(map[string]*gauge)
	requests := make(map[string]*recorder)
	handlers := make(map[string]http.Handler)
	for _, host := range hosts {
		hostGauge, once := &gauge{}, &sync.Once{}
		gauges[host] = hostGauge
		requests[host] = &recorder{}
		handlers[host] = requests[host].wrap(func(writer http.ResponseWriter, request *http.Request) int {
			if request.Header.Get("If-Range") == "" {
				return serveContent(writer, request, content)
			}
			overall.enter()
			defer overall.leave()
			hostGauge.enter()
			defer hostGauge.leave()
			once.Do(func() {
				if arrivals.Add(1) == int32(len(hosts)) {
					close(arrived)
				}
			})
			select {
			case <-arrived:
			case <-time.After(waitLimit):
			}
			return serveContent(writer, request, content)
		})
	}
	serveMirrors(handlers)

	downloaded, progress := download(t, httpclient.NewClient(nil), httpclient.DownloadOptions{
		URLs:           mirrorURLs(hosts...),
		ExpectedBytes:  mergeSize,
		ExpectedSHA256: digest,
	})
	if !bytes.Equal(downloaded, content) {
		t.Fatal("downloaded content differs")
	}
	if overall.peak.Load() < int32(len(hosts)) {
		t.Fatalf("peak concurrent chunk requests = %d, want at least %d", overall.peak.Load(), len(hosts))
	}
	if peak := gauges[hosts[0]].peak.Load(); peak != 1 {
		t.Fatalf("peak concurrent chunk requests to %s = %d, want 1", hosts[0], peak)
	}
	for _, host := range hosts {
		records := requests[host].snapshot()
		checkUserAgents(t, records)
		chunks := 0
		for _, entry := range records {
			if entry.ifRange == "" {
				continue
			}
			chunks++
			if entry.ifRange != contentETag || entry.status != http.StatusPartialContent {
				t.Fatalf("unexpected chunk request to %s: %+v", host, entry)
			}
		}
		if chunks == 0 {
			t.Fatalf("no chunk requests to %s", host)
		}
	}
	final := progress.last()
	checkSources(t, final, mergeSize)
	if len(final.Sources) != len(hosts) {
		t.Fatalf("final sources = %+v, want %d", final.Sources, len(hosts))
	}
}

type abortingWriter struct {
	http.ResponseWriter
	remaining int
}

func (w *abortingWriter) Write(data []byte) (int, error) {
	n, err := w.ResponseWriter.Write(data[:min(len(data), w.remaining)])
	w.remaining -= n
	if w.remaining == 0 {
		w.ResponseWriter.(http.Flusher).Flush()
		panic(http.ErrAbortHandler)
	}
	return n, err
}

func TestDownloadReclaimsFailedMirrorChunks(t *testing.T) {
	content, digest := newContent(contentSize)
	healthy := []string{"mirrors.a.edu.cn", "mirrors.b.edu.cn"}
	const failing = "mirror-failing.example"
	for _, test := range []struct {
		name  string
		serve func(http.ResponseWriter, *http.Request)
	}{
		{
			name: "too many requests",
			serve: func(writer http.ResponseWriter, request *http.Request) {
				writer.WriteHeader(http.StatusTooManyRequests)
			},
		},
		{
			name: "internal server error",
			serve: func(writer http.ResponseWriter, request *http.Request) {
				writer.WriteHeader(http.StatusInternalServerError)
			},
		},
		{
			name: "connection reset",
			serve: func(writer http.ResponseWriter, request *http.Request) {
				serveContent(&abortingWriter{ResponseWriter: writer, remaining: 1 << 20}, request, content)
			},
		},
	} {
		t.Run(test.name, func(t *testing.T) {
			var failed atomic.Int32
			reached := make(chan struct{})
			handlers := map[string]http.Handler{
				failing: http.HandlerFunc(func(writer http.ResponseWriter, request *http.Request) {
					if request.Header.Get("If-Range") == "" {
						serveContent(writer, request, content)
						return
					}
					if failed.Add(1) == 1 {
						close(reached)
					}
					test.serve(writer, request)
				}),
			}
			for _, host := range healthy {
				handlers[host] = http.HandlerFunc(func(writer http.ResponseWriter, request *http.Request) {
					if request.Header.Get("If-Range") != "" {
						select {
						case <-reached:
						case <-time.After(waitLimit):
						}
					}
					serveContent(writer, request, content)
				})
			}
			serveMirrors(handlers)

			downloaded, progress := download(t, httpclient.NewClient(nil), httpclient.DownloadOptions{
				URLs:           mirrorURLs(append([]string{failing}, healthy...)...),
				ExpectedBytes:  contentSize,
				ExpectedSHA256: digest,
			})
			if !bytes.Equal(downloaded, content) {
				t.Fatal("downloaded content differs")
			}
			if count := failed.Load(); count == 0 || count > workerCount {
				t.Fatalf("chunk requests to %s = %d, want between 1 and %d", failing, count, workerCount)
			}
			final := progress.last()
			checkSources(t, final, contentSize)
			for _, entry := range final.Sources {
				if entry.Failed {
					t.Fatalf("final sources = %+v, want only healthy mirrors", final.Sources)
				}
			}
		})
	}
}

func TestDownloadRejectsMismatchedMirror(t *testing.T) {
	content, digest := newContent(contentSize)
	oversized := append(bytes.Clone(content), make([]byte, 1<<20)...)
	const (
		healthy    = "mirrors.example.edu.cn"
		mismatched = "mirror-mismatched.example"
	)
	for _, test := range []struct {
		name          string
		expectedBytes int64
		delayed       string
	}{
		{name: "expected bytes", expectedBytes: contentSize, delayed: healthy},
		{name: "first responder", delayed: mismatched},
	} {
		t.Run(test.name, func(t *testing.T) {
			probed := make(chan struct{})
			var once sync.Once
			requests := &recorder{}
			serveMirrors(map[string]http.Handler{
				healthy: http.HandlerFunc(func(writer http.ResponseWriter, request *http.Request) {
					if request.Header.Get("If-Range") == "" && test.delayed == healthy {
						time.Sleep(200 * time.Millisecond)
					}
					if request.Header.Get("If-Range") != "" {
						select {
						case <-probed:
						case <-time.After(waitLimit):
						}
					}
					serveContent(writer, request, content)
				}),
				mismatched: requests.wrap(func(writer http.ResponseWriter, request *http.Request) int {
					if test.delayed == mismatched {
						time.Sleep(200 * time.Millisecond)
					}
					defer once.Do(func() { close(probed) })
					return serveContent(writer, request, oversized)
				}),
			})

			downloaded, progress := download(t, httpclient.NewClient(nil), httpclient.DownloadOptions{
				URLs:           mirrorURLs(mismatched, healthy),
				ExpectedBytes:  test.expectedBytes,
				ExpectedSHA256: digest,
			})
			if !bytes.Equal(downloaded, content) {
				t.Fatal("downloaded content differs")
			}
			if records := requests.snapshot(); len(records) != 1 || records[0].byteRange != "bytes=0-0" {
				t.Fatalf("requests to %s = %+v, want only the probe", mismatched, records)
			}
			final := progress.last()
			checkSources(t, final, contentSize)
			if len(final.Sources) != 1 || final.Sources[0].Index != 1 {
				t.Fatalf("final sources = %+v, want only %s", final.Sources, healthy)
			}
		})
	}
}
