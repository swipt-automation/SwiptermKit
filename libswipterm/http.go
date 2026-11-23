package libswipterm

import (
	"bytes"
	"context"
	"errors"
	"io"
	"maps"
	"math"
	"net/http"
	"slices"
	"strings"
	"time"

	"github.com/swipt-automation/SwiptermKit/internal/httpclient"
)

const (
	HTTPFailureInsecureURL int32 = iota + 1
	HTTPFailureStatus
	HTTPFailureContentLength
	HTTPFailureChecksum
	HTTPFailureEmptyBody
)

var httpClient = httpclient.NewClient(nil)

type HTTPFailure struct {
	Kind           int32
	URL            string
	Status         int32
	ExpectedBytes  int64
	ActualBytes    int64
	ExpectedSHA256 string
	ActualSHA256   string
}

func newHTTPFailure(err error) *HTTPFailure {
	if insecure, ok := errors.AsType[*httpclient.InsecureURLError](err); ok {
		return &HTTPFailure{Kind: HTTPFailureInsecureURL, URL: insecure.URL.Redacted()}
	}
	if status, ok := errors.AsType[*httpclient.StatusError](err); ok {
		return &HTTPFailure{Kind: HTTPFailureStatus, Status: int32(status.StatusCode)}
	}
	if length, ok := errors.AsType[*httpclient.ContentLengthError](err); ok {
		return &HTTPFailure{Kind: HTTPFailureContentLength, ExpectedBytes: length.Expected, ActualBytes: length.Actual}
	}
	if checksum, ok := errors.AsType[*httpclient.ChecksumError](err); ok {
		return &HTTPFailure{Kind: HTTPFailureChecksum, ExpectedSHA256: checksum.Expected, ActualSHA256: checksum.Actual}
	}
	if errors.Is(err, httpclient.ErrEmptyBody) {
		return &HTTPFailure{Kind: HTTPFailureEmptyBody}
	}
	return nil
}

type HTTPRequest struct {
	request *http.Request
	cancel  context.CancelFunc
	failure *HTTPFailure
}

func NewHTTPRequest(method string, link string, body []byte) (*HTTPRequest, error) {
	ctx, cancel := context.WithCancel(context.Background())
	request, err := http.NewRequestWithContext(ctx, method, link, bytes.NewReader(body))
	if err != nil {
		cancel()
		return nil, err
	}
	return &HTTPRequest{request: request, cancel: cancel}, nil
}

func (r *HTTPRequest) SetHeader(name string, value string) {
	r.request.Header.Set(name, value)
}

func (r *HTTPRequest) Execute(timeoutMilliseconds int64) (*HTTPResponse, error) {
	ctx := r.request.Context()
	if timeoutMilliseconds > 0 {
		var cancel context.CancelFunc
		ctx, cancel = context.WithTimeout(ctx, time.Duration(min(timeoutMilliseconds, math.MaxInt64/int64(time.Millisecond)))*time.Millisecond)
		defer cancel()
	}
	response, err := httpClient.Do(r.request.WithContext(ctx))
	if err != nil {
		r.failure = newHTTPFailure(err)
		return nil, err
	}
	defer response.Body.Close()
	body, err := io.ReadAll(response.Body)
	if err != nil {
		return nil, err
	}
	return &HTTPResponse{
		response: response,
		names:    slices.Sorted(maps.Keys(response.Header)),
		body:     body,
	}, nil
}

func (r *HTTPRequest) Cancel() {
	r.cancel()
}

func (r *HTTPRequest) Failure() *HTTPFailure {
	return r.failure
}

type HTTPResponse struct {
	response *http.Response
	names    []string
	body     []byte
}

func (r *HTTPResponse) StatusCode() int32 {
	return int32(r.response.StatusCode)
}

func (r *HTTPResponse) URL() string {
	return r.response.Request.URL.String()
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

func (r *HTTPResponse) Body() []byte {
	return r.body
}

type HTTPDownloadHandler interface {
	Progress(progress *HTTPDownloadProgress)
	Verifying()
}

type HTTPDownloadProgress struct {
	progress httpclient.Progress
}

func (p *HTTPDownloadProgress) Written() int64 {
	return p.progress.Written
}

func (p *HTTPDownloadProgress) Total() int64 {
	return p.progress.Total
}

func (p *HTTPDownloadProgress) Streams() int32 {
	return int32(p.progress.Streams)
}

func (p *HTTPDownloadProgress) SourceCount() int32 {
	return int32(len(p.progress.Sources))
}

func (p *HTTPDownloadProgress) SourceIndex(index int32) int32 {
	return int32(p.progress.Sources[index].Index)
}

func (p *HTTPDownloadProgress) SourceWritten(index int32) int64 {
	return p.progress.Sources[index].Written
}

func (p *HTTPDownloadProgress) SourceStreams(index int32) int32 {
	return int32(p.progress.Sources[index].Streams)
}

func (p *HTTPDownloadProgress) SourceFailed(index int32) bool {
	return p.progress.Sources[index].Failed
}

type downloadHandler struct {
	HTTPDownloadHandler
}

func (h downloadHandler) Progress(progress httpclient.Progress) {
	h.HTTPDownloadHandler.Progress(&HTTPDownloadProgress{progress: progress})
}

type HTTPDownload struct {
	options httpclient.DownloadOptions
	ctx     context.Context
	cancel  context.CancelFunc
	failure *HTTPFailure
}

func NewHTTPDownload(link string, destination string, expectedBytes int64, expectedSHA256 string) *HTTPDownload {
	ctx, cancel := context.WithCancel(context.Background())
	return &HTTPDownload{
		options: httpclient.DownloadOptions{
			URLs:           []string{link},
			Destination:    destination,
			ExpectedBytes:  expectedBytes,
			ExpectedSHA256: expectedSHA256,
		},
		ctx:    ctx,
		cancel: cancel,
	}
}

func (d *HTTPDownload) AddURL(link string) {
	d.options.URLs = append(d.options.URLs, link)
}

func (d *HTTPDownload) Run(handler HTTPDownloadHandler) error {
	err := httpClient.Download(d.ctx, d.options, downloadHandler{handler})
	if err != nil {
		d.failure = newHTTPFailure(err)
	}
	return err
}

func (d *HTTPDownload) Cancel() {
	d.cancel()
}

func (d *HTTPDownload) Failure() *HTTPFailure {
	return d.failure
}
