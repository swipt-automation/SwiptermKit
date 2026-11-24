package libswipterm

import (
	"bytes"
	"context"
	"errors"
	"io"
	"maps"
	"math"
	"net/http"
	"net/http/httptrace"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"sync/atomic"
	"syscall"
	"time"
)

const (
	userAgent        = "SwiptermKit/1.0"
	readBufferSize   = 64 << 10
	progressInterval = 100 * time.Millisecond
)

var httpClient = func() *http.Client {
	transport := http.DefaultTransport.(*http.Transport).Clone()
	transport.TLSHandshakeTimeout = 0
	return &http.Client{Transport: transport}
}()

type HTTPRequest struct {
	request     *http.Request
	cancel      context.CancelCauseFunc
	timeout     time.Duration
	idle        *time.Timer
	errorNumber int32
}

func NewHTTPRequest(method string, link string, body []byte, timeout float64) (*HTTPRequest, error) {
	ctx, cancel := context.WithCancelCause(context.Background())
	request, err := http.NewRequestWithContext(ctx, method, link, bytes.NewReader(body))
	if err != nil {
		cancel(err)
		return nil, err
	}
	r := &HTTPRequest{request: request, cancel: cancel}
	if timeout > 0 {
		r.timeout = time.Duration(min(timeout, float64(math.MaxInt64/time.Second)) * float64(time.Second))
		r.idle = time.AfterFunc(r.timeout, func() {
			cancel(context.DeadlineExceeded)
		})
		r.idle.Stop()
	}
	return r, nil
}

func (r *HTTPRequest) SetHeader(name string, value string) {
	r.request.Header.Set(name, value)
}

func (r *HTTPRequest) Execute() (*HTTPResponse, error) {
	if r.request.Header.Get("User-Agent") == "" {
		r.request.Header.Set("User-Agent", userAgent)
	}
	request := r.request.WithContext(httptrace.WithClientTrace(r.request.Context(), &httptrace.ClientTrace{
		GotConn: func(httptrace.GotConnInfo) {
			r.arm()
		},
	}))
	r.arm()
	response, err := httpClient.Do(request)
	r.disarm()
	if err != nil {
		return nil, err
	}
	return &HTTPResponse{
		request:  r,
		response: response,
		names:    slices.Sorted(maps.Keys(response.Header)),
	}, nil
}

func (r *HTTPRequest) Cancel() {
	r.cancel(context.Canceled)
}

func (r *HTTPRequest) TimedOut() bool {
	return errors.Is(context.Cause(r.request.Context()), context.DeadlineExceeded)
}

func (r *HTTPRequest) ErrorNumber() int32 {
	return r.errorNumber
}

func (r *HTTPRequest) arm() {
	if r.idle != nil {
		r.idle.Reset(r.timeout)
	}
}

func (r *HTTPRequest) disarm() {
	if r.idle != nil {
		r.idle.Stop()
	}
}

func (r *HTTPRequest) fileError(err error) error {
	if errno, ok := errors.AsType[syscall.Errno](err); ok {
		r.errorNumber = int32(errno)
	}
	return err
}

type HTTPDownloadHandler interface {
	Progress(written int64, total int64)
}

type HTTPResponse struct {
	request  *HTTPRequest
	response *http.Response
	names    []string
	chunk    []byte
	closed   atomic.Bool
}

func (r *HTTPResponse) StatusCode() int32 {
	return int32(r.response.StatusCode)
}

func (r *HTTPResponse) URL() string {
	return r.response.Request.URL.String()
}

func (r *HTTPResponse) Proto() string {
	return r.response.Proto
}

func (r *HTTPResponse) HeaderCount() int32 {
	return int32(len(r.names))
}

func (r *HTTPResponse) HeaderName(index int32) string {
	return r.names[index]
}

func (r *HTTPResponse) HeaderValue(index int32) string {
	return strings.Join(r.response.Header.Values(r.names[index]), ", ")
}

func (r *HTTPResponse) Read(maxLength int64) error {
	buffer := make([]byte, maxLength)
	for {
		n, err := r.read(buffer)
		if n > 0 {
			r.chunk = buffer[:n]
			return nil
		}
		if errors.Is(err, io.EOF) || err != nil && r.closed.Load() {
			r.chunk = nil
			return nil
		}
		if err != nil {
			return err
		}
	}
}

func (r *HTTPResponse) Chunk() []byte {
	return r.chunk
}

func (r *HTTPResponse) Download(destination string, handler HTTPDownloadHandler) error {
	defer r.Close()
	file, err := os.CreateTemp(filepath.Dir(destination), "."+filepath.Base(destination)+".*")
	if err != nil {
		return r.request.fileError(err)
	}
	err = r.transfer(file, handler)
	closeErr := file.Close()
	if err == nil && closeErr != nil {
		err = r.request.fileError(closeErr)
	}
	if err == nil {
		err = os.Rename(file.Name(), destination)
		if err != nil {
			err = r.request.fileError(err)
		}
	}
	if err != nil {
		os.Remove(file.Name())
	}
	return err
}

func (r *HTTPResponse) transfer(file *os.File, handler HTTPDownloadHandler) error {
	buffer := make([]byte, readBufferSize)
	var (
		written  int64
		reported time.Time
	)
	for {
		n, err := r.read(buffer)
		if n > 0 {
			_, writeErr := file.Write(buffer[:n])
			if writeErr != nil {
				return r.request.fileError(writeErr)
			}
			written += int64(n)
			if handler != nil && time.Since(reported) >= progressInterval {
				reported = time.Now()
				handler.Progress(written, r.response.ContentLength)
			}
		}
		if errors.Is(err, io.EOF) {
			if handler != nil {
				handler.Progress(written, r.response.ContentLength)
			}
			return nil
		}
		if err != nil {
			return err
		}
	}
}

func (r *HTTPResponse) read(buffer []byte) (int, error) {
	r.request.arm()
	n, err := r.response.Body.Read(buffer)
	r.request.disarm()
	return n, err
}

func (r *HTTPResponse) Close() {
	if r.closed.Swap(true) {
		return
	}
	r.request.Cancel()
	r.response.Body.Close()
}
