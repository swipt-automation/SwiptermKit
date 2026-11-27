package libswipterm

import (
	"context"
	"errors"
	"io"
	"net/http"
	"net/http/httptrace"
	"os"
	"path/filepath"
	"syscall"
	"time"

	E "github.com/sagernet/sing/common/exceptions"
)

const (
	userAgent        = "SwiptermKit/1.0"
	idleTimeout      = 60 * time.Second
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
	idle        *time.Timer
	errorNumber int32
}

func NewHTTPRequest(link string) (*HTTPRequest, error) {
	ctx, cancel := context.WithCancelCause(context.Background())
	request, err := http.NewRequestWithContext(ctx, http.MethodGet, link, nil)
	if err != nil {
		cancel(err)
		return nil, err
	}
	request.Header.Set("User-Agent", userAgent)
	r := &HTTPRequest{request: request, cancel: cancel}
	r.idle = time.AfterFunc(idleTimeout, func() {
		cancel(context.DeadlineExceeded)
	})
	r.idle.Stop()
	return r, nil
}

func (r *HTTPRequest) Execute() (*HTTPResponse, error) {
	request := r.request.WithContext(httptrace.WithClientTrace(r.request.Context(), &httptrace.ClientTrace{
		GotConn: func(httptrace.GotConnInfo) {
			r.idle.Reset(idleTimeout)
		},
	}))
	r.idle.Reset(idleTimeout)
	response, err := httpClient.Do(request)
	r.idle.Stop()
	if err != nil {
		return nil, err
	}
	return &HTTPResponse{request: r, response: response}, nil
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
}

func (r *HTTPResponse) StatusCode() int32 {
	return int32(r.response.StatusCode)
}

func (r *HTTPResponse) URL() string {
	return r.response.Request.URL.String()
}

func (r *HTTPResponse) ReadAll() ([]byte, error) {
	defer r.close()
	return io.ReadAll(idleReader{r})
}

func (r *HTTPResponse) Download(destination string, handler HTTPDownloadHandler) error {
	defer r.close()
	file, err := os.CreateTemp(filepath.Dir(destination), "."+filepath.Base(destination)+".*")
	if err != nil {
		return r.request.fileError(err)
	}
	err = r.transfer(file, handler)
	err = E.Append(err, file.Close(), func(err error) error {
		return r.request.fileError(E.Cause(err, "close ", file.Name()))
	})
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
		n, err := idleReader{r}.Read(buffer)
		if n > 0 {
			_, writeErr := file.Write(buffer[:n])
			if writeErr != nil {
				return r.request.fileError(writeErr)
			}
			written += int64(n)
			if time.Since(reported) >= progressInterval {
				reported = time.Now()
				handler.Progress(written, r.response.ContentLength)
			}
		}
		if errors.Is(err, io.EOF) {
			handler.Progress(written, r.response.ContentLength)
			return nil
		}
		if err != nil {
			return err
		}
	}
}

func (r *HTTPResponse) close() {
	r.request.Cancel()
	r.response.Body.Close()
}

type idleReader struct {
	response *HTTPResponse
}

func (r idleReader) Read(buffer []byte) (int, error) {
	r.response.request.idle.Reset(idleTimeout)
	n, err := r.response.response.Body.Read(buffer)
	r.response.request.idle.Stop()
	return n, err
}
