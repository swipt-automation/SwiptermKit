package httpclient

import (
	"bufio"
	"context"
	"errors"
	"io"
	"mime"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"slices"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	E "github.com/sagernet/sing/common/exceptions"
	F "github.com/sagernet/sing/common/format"
)

const (
	retryLimit       = 5
	retryDelay       = time.Second
	stallTimeout     = 30 * time.Second
	progressInterval = 100 * time.Millisecond
	readBufferSize   = 64 << 10
	writeBufferSize  = 1 << 20
	parallelMinimum  = 16 << 20
	parallelChunk    = 4 << 20
	parallelWorkers  = 4
	probeConcurrency = 8
	streamLimit      = 8
)

var errStalled = E.New("http: no data received for ", stallTimeout)

type DownloadOptions struct {
	URL           string
	URLs          []string
	Destination   string
	ExpectedBytes int64
}

type SourceProgress struct {
	Index   int
	Written int64
	Streams int
	Failed  bool
}

type Progress struct {
	Written int64
	Total   int64
	Streams int
	Sources []SourceProgress
}

type DownloadHandler interface {
	Progress(progress Progress)
}

type source struct {
	index     int
	url       *url.URL
	academic  bool
	validator string
	written   atomic.Int64
	streams   int
	failed    bool
}

type download struct {
	client     *http.Client
	options    DownloadOptions
	handler    DownloadHandler
	file       *os.File
	writer     *bufio.Writer
	written    atomic.Int64
	total      int64
	sources    []*source
	progressed bool
	reported   time.Time
}

type probeResult struct {
	index    int
	response *http.Response
	err      error
}

type fetchResult struct {
	source *source
	index  int
	err    error
}

type pool struct {
	download  *download
	ctx       context.Context
	cancel    context.CancelFunc
	fetching  sync.WaitGroup
	results   chan fetchResult
	pending   []int
	completed []bool
	remaining int
	active    int
}

func (c *Client) Download(ctx context.Context, options DownloadOptions, handler DownloadHandler) error {
	links := options.URLs
	if len(links) == 0 {
		links = []string{options.URL}
	}
	targets := make([]*url.URL, 0, len(links))
	for _, link := range links {
		target, err := url.Parse(link)
		if err != nil {
			return err
		}
		targets = append(targets, target)
	}
	file, err := os.CreateTemp(filepath.Dir(options.Destination), "."+filepath.Base(options.Destination)+".*")
	if err != nil {
		return err
	}
	d := &download{
		client:  &c.client,
		options: options,
		handler: handler,
		file:    file,
		writer:  bufio.NewWriterSize(file, writeBufferSize),
		total:   -1,
	}
	err = E.Errors(d.run(ctx, targets), file.Close())
	if err == nil {
		err = os.Rename(file.Name(), options.Destination)
	}
	if err != nil {
		os.Remove(file.Name())
	}
	return err
}

func (d *download) run(ctx context.Context, targets []*url.URL) error {
	var target *source
	if len(targets) > 1 {
		var err error
		target, err = d.merge(ctx, targets)
		if err != nil {
			return err
		}
		if target == nil {
			return d.finish()
		}
	} else {
		target = &source{url: targets[0]}
		if !academic(target.url) {
			completed, err := d.parallel(ctx, target)
			if err != nil {
				return err
			}
			if completed {
				return d.finish()
			}
		}
	}
	return d.stream(ctx, target)
}

func (d *download) stream(ctx context.Context, target *source) error {
	failures := 0
	for {
		d.progressed = false
		retry, err := d.attempt(ctx, target)
		if err == nil {
			return d.finish()
		}
		if ctx.Err() != nil {
			return ctx.Err()
		}
		if !retry {
			return err
		}
		if d.written.Load() == 0 {
			return &TransientError{Cause: err}
		}
		if d.progressed {
			failures = 0
		}
		failures++
		if failures > retryLimit {
			return err
		}
		select {
		case <-time.After(retryDelay << (failures - 1)):
		case <-ctx.Done():
			return ctx.Err()
		}
	}
}

func (d *download) parallel(ctx context.Context, target *source) (bool, error) {
	response, err := d.get(ctx, target.url, "bytes=0-0", "")
	if err != nil {
		return false, ctx.Err()
	}
	response.Body.Close()
	if response.StatusCode == http.StatusForbidden {
		return false, &StatusError{StatusCode: response.StatusCode}
	}
	start, end, total, ok := parseContentRange(response.Header.Get("Content-Range"))
	validator := strongValidator(response.Header)
	if response.StatusCode != http.StatusPartialContent || !ok || start != 0 || end != 0 || total < parallelMinimum ||
		validator == "" || academic(response.Request.URL) {
		return false, nil
	}
	d.total = total
	err = d.checkTotal()
	if err != nil {
		return false, err
	}
	target.validator = validator
	d.sources = append(d.sources, target)
	chunks := d.newPool(ctx)
	defer chunks.close()
	chunks.schedule()
	ticker := time.NewTicker(progressInterval)
	defer ticker.Stop()
	for chunks.remaining > 0 {
		select {
		case <-ticker.C:
			d.report(true)
		case result := <-chunks.results:
			chunks.complete(result)
			if result.err == nil {
				chunks.schedule()
				continue
			}
			if ctx.Err() != nil {
				return false, ctx.Err()
			}
			if status, isStatus := errors.AsType[*StatusError](result.err); isStatus && status.StatusCode == http.StatusForbidden {
				return false, result.err
			}
			chunks.close()
			written := int64(slices.Index(chunks.completed, false)) * parallelChunk
			d.written.Store(written)
			target.written.Store(written)
			_, err = d.file.Seek(written, io.SeekStart)
			return false, err
		case <-ctx.Done():
			return false, ctx.Err()
		}
	}
	return true, nil
}

func (d *download) merge(ctx context.Context, targets []*url.URL) (*source, error) {
	probeCtx, cancelProbes := context.WithCancel(ctx)
	probes := make(chan probeResult, len(targets))
	var probing sync.WaitGroup
	defer probing.Wait()
	defer cancelProbes()
	slots := make(chan struct{}, probeConcurrency)
	for index, target := range targets {
		probing.Go(func() {
			select {
			case slots <- struct{}{}:
			case <-probeCtx.Done():
				return
			}
			response, err := d.get(probeCtx, target, "bytes=0-0", "")
			<-slots
			if err == nil {
				response.Body.Close()
			}
			probes <- probeResult{index: index, response: response, err: err}
		})
	}
	var (
		chunks   *pool
		fallback *source
		failures []error
	)
	defer func() {
		if chunks != nil {
			chunks.close()
		}
	}()
	ticker := time.NewTicker(progressInterval)
	defer ticker.Stop()
	for unanswered := len(targets); ; {
		var results <-chan fetchResult
		if chunks != nil {
			if chunks.remaining == 0 {
				return nil, nil
			}
			results = chunks.results
		}
		if unanswered == 0 && (chunks == nil || chunks.active == 0) {
			if chunks == nil && fallback != nil {
				return fallback, nil
			}
			return nil, E.Errors(failures...)
		}
		select {
		case result := <-probes:
			unanswered--
			target := targets[result.index]
			if result.err != nil {
				failures = append(failures, E.Cause(result.err, "probe ", target.Host))
				continue
			}
			response := result.response
			candidate := &source{
				index:     result.index,
				url:       target,
				academic:  academic(target) || academic(response.Request.URL),
				validator: strongValidator(response.Header),
			}
			err := d.admit(response)
			if err == nil && response.StatusCode == http.StatusOK {
				if fallback == nil {
					fallback = candidate
				}
				continue
			}
			if err != nil {
				failures = append(failures, E.Cause(err, "probe ", target.Host))
				continue
			}
			d.sources = append(d.sources, candidate)
			if chunks == nil {
				chunks = d.newPool(ctx)
			}
			chunks.schedule()
		case result := <-results:
			chunks.complete(result)
			if result.err != nil {
				result.source.failed = true
				failures = append(failures, E.Cause(result.err, "download from ", result.source.url.Host))
			}
			chunks.schedule()
		case <-ticker.C:
			d.report(true)
		case <-ctx.Done():
			return nil, ctx.Err()
		}
	}
}

func (d *download) admit(response *http.Response) error {
	mediaType, _, _ := mime.ParseMediaType(response.Header.Get("Content-Type"))
	if mediaType == "text/html" {
		return E.New("http: unexpected Content-Type: ", mediaType)
	}
	switch response.StatusCode {
	case http.StatusOK:
		return nil
	case http.StatusPartialContent:
	default:
		return &StatusError{StatusCode: response.StatusCode}
	}
	value := response.Header.Get("Content-Range")
	start, end, total, ok := parseContentRange(value)
	if !ok || start != 0 || end != 0 || total <= 0 {
		return E.New("http: unexpected Content-Range: ", value)
	}
	if d.options.ExpectedBytes > 0 && total != d.options.ExpectedBytes {
		return &ContentLengthError{Expected: d.options.ExpectedBytes, Actual: total}
	}
	if d.total < 0 {
		d.total = total
	} else if total != d.total {
		return &ContentLengthError{Expected: d.total, Actual: total}
	}
	return nil
}

func (d *download) newPool(ctx context.Context) *pool {
	ctx, cancel := context.WithCancel(ctx)
	count := int((d.total + parallelChunk - 1) / parallelChunk)
	chunks := &pool{
		download:  d,
		ctx:       ctx,
		cancel:    cancel,
		results:   make(chan fetchResult, streamLimit),
		pending:   make([]int, count),
		completed: make([]bool, count),
		remaining: count,
	}
	for index := range chunks.pending {
		chunks.pending[index] = index
	}
	return chunks
}

func (p *pool) schedule() {
	healthy := 0
	for _, s := range p.download.sources {
		if !s.failed && !s.academic {
			healthy++
		}
	}
	for scheduled := true; scheduled; {
		scheduled = false
		for _, s := range p.download.sources {
			if len(p.pending) == 0 || p.active == streamLimit {
				return
			}
			limit := 1
			if !s.academic && healthy == 1 {
				limit = parallelWorkers
			}
			if s.failed || s.streams >= limit {
				continue
			}
			index := p.pending[0]
			p.pending = p.pending[1:]
			s.streams++
			p.active++
			scheduled = true
			p.fetching.Go(func() {
				p.results <- fetchResult{source: s, index: index, err: p.download.fetch(p.ctx, s, index)}
			})
		}
	}
}

func (p *pool) complete(result fetchResult) {
	result.source.streams--
	p.active--
	if result.err != nil {
		p.pending = slices.Insert(p.pending, 0, result.index)
		return
	}
	p.completed[result.index] = true
	p.remaining--
}

func (p *pool) close() {
	p.cancel()
	p.fetching.Wait()
	for p.active > 0 {
		p.complete(<-p.results)
	}
}

func (d *download) fetch(ctx context.Context, target *source, index int) error {
	start := int64(index) * parallelChunk
	end := min(start+parallelChunk, d.total) - 1
	ctx, cancel := context.WithCancelCause(ctx)
	defer cancel(nil)
	response, err := d.get(ctx, target.url, F.ToString("bytes=", start, "-", end), target.validator)
	if err != nil {
		return err
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusPartialContent {
		return &StatusError{StatusCode: response.StatusCode}
	}
	value := response.Header.Get("Content-Range")
	first, last, total, ok := parseContentRange(value)
	if !ok || first != start || last != end || total != d.total {
		return E.New("http: unexpected Content-Range: ", value)
	}
	var written int64
	_, err = transfer(ctx, cancel, response.Body, io.NewOffsetWriter(d.file, start), func(n int64) {
		written += n
		target.written.Add(n)
		d.written.Add(n)
	})
	if err == nil && written != end-start+1 {
		err = &ContentLengthError{Expected: end - start + 1, Actual: written}
	}
	if err != nil {
		target.written.Add(-written)
		d.written.Add(-written)
	}
	return err
}

func (d *download) attempt(ctx context.Context, target *source) (bool, error) {
	ctx, cancel := context.WithCancelCause(ctx)
	defer cancel(nil)
	var byteRange string
	if written := d.written.Load(); written > 0 && target.validator != "" {
		byteRange = F.ToString("bytes=", written, "-")
	}
	response, err := d.get(ctx, target.url, byteRange, target.validator)
	if err != nil {
		return true, err
	}
	defer response.Body.Close()
	switch code := response.StatusCode; {
	case code == http.StatusPartialContent:
		err = d.resume(response)
	case code == http.StatusRequestedRangeNotSatisfiable:
		return false, d.complete(response)
	case code >= 200 && code < 300:
		err = d.restart(response, target)
	default:
		return code == http.StatusRequestTimeout || code == http.StatusTooManyRequests || code >= 500, &StatusError{StatusCode: code}
	}
	if err != nil {
		return false, err
	}
	if !slices.Contains(d.sources, target) {
		d.sources = append(d.sources, target)
	}
	target.streams = 1
	retry, err := transfer(ctx, cancel, response.Body, d.writer, func(n int64) {
		d.written.Add(n)
		target.written.Add(n)
		d.progressed = true
		d.report(false)
	})
	target.streams = 0
	flushErr := d.writer.Flush()
	if flushErr != nil {
		return false, flushErr
	}
	return retry, err
}

func (d *download) get(ctx context.Context, target *url.URL, byteRange string, validator string) (*http.Response, error) {
	request, err := http.NewRequestWithContext(ctx, http.MethodGet, target.String(), nil)
	if err != nil {
		return nil, err
	}
	request.Header.Set("Accept-Encoding", "identity")
	request.Header.Set("User-Agent", userAgent)
	if byteRange != "" {
		request.Header.Set("Range", byteRange)
		if validator != "" {
			request.Header.Set("If-Range", validator)
		}
	}
	return d.client.Do(request)
}

func (d *download) restart(response *http.Response, target *source) error {
	_, err := d.file.Seek(0, io.SeekStart)
	if err != nil {
		return err
	}
	err = d.file.Truncate(0)
	if err != nil {
		return err
	}
	d.written.Store(0)
	target.written.Store(0)
	d.total = response.ContentLength
	target.validator = strongValidator(response.Header)
	return d.checkTotal()
}

func (d *download) resume(response *http.Response) error {
	value := response.Header.Get("Content-Range")
	start, end, total, ok := parseContentRange(value)
	if ok && total >= 0 && d.total >= 0 && total != d.total {
		ok = false
	}
	if ok && total >= 0 {
		d.total = total
	}
	if !ok || start != d.written.Load() || (d.total >= 0 && end != d.total-1) {
		return E.New("http: unexpected Content-Range: ", value)
	}
	return d.checkTotal()
}

func (d *download) complete(response *http.Response) error {
	_, _, size, ok := parseContentRange(response.Header.Get("Content-Range"))
	total := d.expectedTotal()
	if total < 0 && ok {
		total = size
	}
	written := d.written.Load()
	if written > 0 && written == total && (!ok || size < 0 || size == total) {
		return nil
	}
	return &StatusError{StatusCode: response.StatusCode}
}

func (d *download) checkTotal() error {
	if d.options.ExpectedBytes > 0 && d.total >= 0 && d.total != d.options.ExpectedBytes {
		return &ContentLengthError{Expected: d.options.ExpectedBytes, Actual: d.total}
	}
	return nil
}

func transfer(ctx context.Context, cancel context.CancelCauseFunc, body io.Reader, writer io.Writer, written func(int64)) (bool, error) {
	stall := time.AfterFunc(stallTimeout, func() {
		cancel(errStalled)
	})
	defer stall.Stop()
	buffer := make([]byte, readBufferSize)
	for {
		n, readErr := body.Read(buffer)
		if n > 0 {
			stall.Reset(stallTimeout)
			_, err := writer.Write(buffer[:n])
			if err != nil {
				return false, err
			}
			written(int64(n))
		}
		if readErr == nil {
			continue
		}
		if readErr == io.EOF {
			return false, nil
		}
		if errors.Is(context.Cause(ctx), errStalled) {
			readErr = errStalled
		}
		return true, readErr
	}
}

func (d *download) finish() error {
	written := d.written.Load()
	if written == 0 {
		return ErrEmptyBody
	}
	if d.total >= 0 && written != d.total {
		return &ContentLengthError{Expected: d.total, Actual: written}
	}
	if d.options.ExpectedBytes > 0 && written != d.options.ExpectedBytes {
		return &ContentLengthError{Expected: d.options.ExpectedBytes, Actual: written}
	}
	d.report(true)
	return nil
}

func (d *download) expectedTotal() int64 {
	if d.total >= 0 {
		return d.total
	}
	if d.options.ExpectedBytes > 0 {
		return d.options.ExpectedBytes
	}
	return -1
}

func (d *download) report(force bool) {
	now := time.Now()
	if !force && now.Sub(d.reported) < progressInterval {
		return
	}
	d.reported = now
	progress := Progress{Written: d.written.Load(), Total: d.expectedTotal()}
	for _, s := range d.sources {
		written := s.written.Load()
		if s.streams == 0 && written == 0 {
			continue
		}
		progress.Streams += s.streams
		progress.Sources = append(progress.Sources, SourceProgress{
			Index:   s.index,
			Written: written,
			Streams: s.streams,
			Failed:  s.failed,
		})
	}
	d.handler.Progress(progress)
}

func strongValidator(header http.Header) string {
	validator := header.Get("ETag")
	if validator == "" || strings.HasPrefix(validator, "W/") {
		validator = header.Get("Last-Modified")
	}
	return validator
}

func academic(target *url.URL) bool {
	return strings.HasSuffix(strings.ToLower(target.Hostname()), ".edu.cn")
}

func parseContentRange(value string) (start int64, end int64, total int64, ok bool) {
	spec, found := strings.CutPrefix(value, "bytes ")
	if !found {
		return
	}
	span, size, found := strings.Cut(spec, "/")
	if !found {
		return
	}
	total = -1
	var err error
	if size != "*" {
		total, err = strconv.ParseInt(size, 10, 64)
		if err != nil {
			return
		}
	}
	if span == "*" {
		return -1, -1, total, true
	}
	first, last, found := strings.Cut(span, "-")
	if !found {
		return
	}
	start, err = strconv.ParseInt(first, 10, 64)
	if err != nil {
		return
	}
	end, err = strconv.ParseInt(last, 10, 64)
	if err != nil {
		return
	}
	return start, end, total, true
}
