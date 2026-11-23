package main

import (
	"compress/gzip"
	"context"
	"errors"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"slices"
	"strconv"
	"strings"
	"time"

	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/swipt-automation/SwiptermKit/internal/httpclient"
)

const (
	e2fsprogsVersion = "1.47.4"
	e2fsprogsURL     = "https://cdn.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v" + e2fsprogsVersion + "/e2fsprogs-" + e2fsprogsVersion + ".tar.xz"
	sourceStamp      = ".swipterm-stamp"
	downloadRetries  = 3
)

var (
	e2fsprogsConfigure = []string{
		"--disable-nls", "--disable-fuse2fs", "--disable-debugfs", "--disable-imager", "--disable-resizer",
		"--disable-defrag", "--disable-uuidd", "--disable-e2initrd-helper", "--disable-tdb", "--disable-backtrace",
		"--disable-mmp", "--disable-bmap-stats", "--disable-rpath", "--disable-elf-shlibs",
		"--enable-libuuid", "--enable-libblkid", "--without-libarchive",
		"--without-udev-rules-dir", "--without-crond-dir", "--without-systemd-unit-dir",
		"CFLAGS=-Os -ffunction-sections -fdata-sections",
		"LDFLAGS_STATIC=-static -Wl,--gc-sections",
	}
	staticCFlags = []string{
		"-static", "-Os", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
		"-fno-asynchronous-unwind-tables", "-ffunction-sections", "-fdata-sections",
	}
	staticLDFlags = []string{"-static", "-Wl,--gc-sections"}
)

type initramfsEntry struct {
	path   string
	mode   uint32
	source string
}

var initramfs = []initramfsEntry{
	{"dev", cpioDirectory | 0o755, ""},
	{"mnt", cpioDirectory | 0o755, ""},
	{"mnt/root", cpioDirectory | 0o755, ""},
	{"proc", cpioDirectory | 0o555, ""},
	{"run", cpioDirectory | 0o755, ""},
	{"sbin", cpioDirectory | 0o755, ""},
	{"sys", cpioDirectory | 0o555, ""},
	{"init", cpioRegular | 0o755, "swiptermd"},
	{"cmd_stub", cpioRegular | 0o755, "cmd_stub"},
	{"guest_clip", cpioRegular | 0o755, "guest_clip"},
	{"sbin/modprobe", cpioRegular | 0o755, "modprobe"},
	{"sbin/e2fsck", cpioRegular | 0o755, "e2fsprogs/e2fsck"},
	{"sbin/tune2fs", cpioRegular | 0o755, "e2fsprogs/tune2fs"},
}

type builder struct {
	work      string
	jobs      string
	epoch     int64
	compiler  []string
	e2fsprogs string
}

func newBuilder() *builder {
	cache, err := os.UserCacheDir()
	if err != nil {
		cache = os.TempDir()
	}
	work := filepath.Join(cache, "swipterm-guest")
	b := &builder{
		work:      work,
		jobs:      strconv.Itoa(runtime.NumCPU()),
		compiler:  strings.Fields(os.Getenv("CC")),
		e2fsprogs: filepath.Join(work, "e2fsprogs-"+e2fsprogsVersion),
	}
	b.epoch, _ = strconv.ParseInt(os.Getenv("SOURCE_DATE_EPOCH"), 10, 64)
	if len(b.compiler) == 0 {
		b.compiler = []string{"gcc"}
		if _, err = exec.LookPath("musl-gcc"); err == nil {
			b.compiler = []string{"musl-gcc"}
		}
	}
	return b
}

func (b *builder) build() error {
	if runtime.GOOS != "linux" {
		return E.New("guest artifacts build on Linux")
	}
	err := os.MkdirAll(buildPath, 0o755)
	if err != nil {
		return err
	}
	err = b.prepareSource(e2fsprogsURL, b.e2fsprogs)
	if err != nil {
		return err
	}
	for _, step := range []func() error{
		b.buildE2fsprogs,
		b.buildDaemons,
		b.buildInitramfs,
	} {
		err = step()
		if err != nil {
			return err
		}
	}
	return nil
}

func (b *builder) prepareSource(link string, directory string) error {
	stamp, _ := os.ReadFile(filepath.Join(directory, sourceStamp))
	if strings.TrimSpace(string(stamp)) == link {
		return nil
	}
	tarball := filepath.Join(b.work, filepath.Base(link))
	if _, err := os.Stat(tarball); err != nil {
		err = os.MkdirAll(b.work, 0o755)
		if err != nil {
			return err
		}
		os.Stderr.WriteString("download " + link + "\n")
		client := httpclient.NewClient(nil)
		for retry := 0; ; retry++ {
			err = client.Download(context.Background(), httpclient.DownloadOptions{
				URL:         link,
				Destination: tarball,
			}, silentDownload{})
			if _, transient := errors.AsType[*httpclient.TransientError](err); !transient || retry == downloadRetries {
				break
			}
			time.Sleep(time.Second << retry)
		}
		if err != nil {
			return E.Cause(err, "download ", link)
		}
	}
	err := os.RemoveAll(directory)
	if err == nil {
		err = os.MkdirAll(directory, 0o755)
	}
	if err == nil {
		err = run(exec.Command("tar", "-xf", tarball, "-C", directory, "--strip-components=1"))
	}
	if err != nil {
		return err
	}
	return os.WriteFile(filepath.Join(directory, sourceStamp), []byte(link+"\n"), 0o644)
}

func (b *builder) buildE2fsprogs() error {
	output := filepath.Join(buildPath, "e2fsprogs")
	stampPath := filepath.Join(output, "configure")
	stamp, _ := os.ReadFile(stampPath)
	configuration := strings.Join(append([]string{e2fsprogsURL}, e2fsprogsConfigure...), "\n")
	if string(stamp) == configuration {
		return nil
	}
	directory := filepath.Join(b.e2fsprogs, "build")
	err := os.RemoveAll(directory)
	if err == nil {
		err = os.MkdirAll(directory, 0o755)
	}
	if err != nil {
		return err
	}
	configure := exec.Command("../configure", append([]string{"-q"}, e2fsprogsConfigure...)...)
	configure.Dir = directory
	for _, command := range []*exec.Cmd{
		configure,
		exec.Command("make", "-C", directory, "-s", "-j"+b.jobs, "libs"),
		exec.Command("make", "-C", filepath.Join(directory, "misc"), "-s", "tune2fs.static"),
		exec.Command("make", "-C", filepath.Join(directory, "e2fsck"), "-s", "e2fsck.static"),
	} {
		err = run(command)
		if err != nil {
			return err
		}
	}
	for name, built := range map[string]string{
		"tune2fs": filepath.Join(directory, "misc", "tune2fs.static"),
		"e2fsck":  filepath.Join(directory, "e2fsck", "e2fsck.static"),
	} {
		err = installExecutable(built, filepath.Join(output, name))
		if err != nil {
			return err
		}
	}
	return os.WriteFile(stampPath, []byte(configuration), 0o644)
}

func (b *builder) buildDaemons() error {
	sources, err := filepath.Glob(filepath.Join(guestPath, "swiptermd", "*.c"))
	if err != nil {
		return err
	}
	standalone := []string{"cmd_stub", "guest_clip", "modprobe"}
	daemon := common.Filter(sources, func(source string) bool {
		return !slices.Contains(standalone, strings.TrimSuffix(filepath.Base(source), ".c"))
	})
	programs := map[string][]string{"swiptermd": daemon}
	for _, name := range standalone {
		programs[name] = []string{filepath.Join(guestPath, "swiptermd", name+".c")}
	}
	for name, programSources := range programs {
		output := filepath.Join(buildPath, name)
		arguments := slices.Concat(b.compiler[1:], staticCFlags, programSources, []string{"-o", output}, staticLDFlags)
		err = run(exec.Command(b.compiler[0], arguments...))
		if err == nil {
			err = run(exec.Command("strip", "-s", output))
		}
		if err != nil {
			return err
		}
	}
	return nil
}

func (b *builder) buildInitramfs() error {
	entries := make([]cpioEntry, 0, len(initramfs))
	for _, entry := range initramfs {
		var data []byte
		if entry.source != "" {
			var err error
			data, err = os.ReadFile(filepath.Join(buildPath, entry.source))
			if err != nil {
				return err
			}
		}
		entries = append(entries, cpioEntry{name: entry.path, mode: entry.mode, data: data})
	}
	return writeGzip(filepath.Join(buildPath, "initramfs.cpio.gz"), func(writer io.Writer) error {
		return writeCPIO(writer, entries, b.epoch)
	})
}

func writeGzip(path string, write func(io.Writer) error) error {
	file, err := os.Create(path)
	if err != nil {
		return err
	}
	writer, err := gzip.NewWriterLevel(file, gzip.BestCompression)
	if err == nil {
		err = write(writer)
		err = E.Errors(err, writer.Close())
	}
	err = E.Errors(err, file.Close())
	if err != nil {
		os.Remove(path)
	}
	return err
}

func installExecutable(source string, destination string) error {
	err := copyFile(source, destination, 0o755)
	if err != nil {
		return err
	}
	return run(exec.Command("strip", "-s", destination))
}

func run(command *exec.Cmd) error {
	if command.Stdout == nil {
		command.Stdout = os.Stdout
	}
	command.Stderr = os.Stderr
	err := command.Run()
	if err != nil {
		return E.Cause(err, "run ", strings.Join(command.Args, " "))
	}
	return nil
}

type silentDownload struct{}

func (silentDownload) Progress(httpclient.Progress) {}
