package main

import (
	"bufio"
	"compress/gzip"
	"context"
	"errors"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
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
	linuxVersion     = "6.12"
	linuxSHA256      = "b1a2562be56e42afb3f8489d4c2a7ac472ac23098f1ef1c1e40da601f54625eb"
	linuxURL         = "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-" + linuxVersion + ".tar.xz"
	e2fsprogsVersion = "1.47.4"
	e2fsprogsSHA256  = "fd5bf388cbdbe006a3d3b318d983b2948382440acc85a87f1e7d108653e8db0b"
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
	configLine    = regexp.MustCompile(`^(?:(CONFIG_[A-Za-z0-9_]+)=.*|# (CONFIG_[A-Za-z0-9_]+) is not set)$`)
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
	linux     string
	kbuild    string
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
		linux:     filepath.Join(work, "linux-"+linuxVersion),
		kbuild:    filepath.Join(work, "kbuild-"+linuxVersion),
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
	for _, source := range []struct {
		url    string
		sha256 string
		path   string
	}{
		{linuxURL, linuxSHA256, b.linux},
		{e2fsprogsURL, e2fsprogsSHA256, b.e2fsprogs},
	} {
		err = b.prepareSource(source.url, source.sha256, source.path)
		if err != nil {
			return err
		}
	}
	for _, step := range []func() error{
		b.buildE2fsprogs,
		b.buildDaemons,
		b.buildInitramfs,
		b.configureKernel,
		b.buildKernel,
	} {
		err = step()
		if err != nil {
			return err
		}
	}
	return nil
}

func (b *builder) prepareSource(link string, sha256 string, directory string) error {
	stamp, _ := os.ReadFile(filepath.Join(directory, sourceStamp))
	if strings.TrimSpace(string(stamp)) == sha256 {
		return nil
	}
	tarball := filepath.Join(b.work, filepath.Base(link))
	if sum, err := fileSHA256(tarball); err != nil || sum != sha256 {
		err = os.MkdirAll(b.work, 0o755)
		if err != nil {
			return err
		}
		os.Stderr.WriteString("download " + link + "\n")
		client := httpclient.NewClient(nil)
		for retry := 0; ; retry++ {
			err = client.Download(context.Background(), httpclient.DownloadOptions{
				URL:            link,
				Destination:    tarball,
				ExpectedSHA256: sha256,
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
	return os.WriteFile(filepath.Join(directory, sourceStamp), []byte(sha256+"\n"), 0o644)
}

func (b *builder) buildE2fsprogs() error {
	output := filepath.Join(buildPath, "e2fsprogs")
	stampPath := filepath.Join(output, "configure")
	stamp, _ := os.ReadFile(stampPath)
	configuration := strings.Join(append([]string{e2fsprogsSHA256}, e2fsprogsConfigure...), "\n")
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

func (b *builder) kernelMake(arguments ...string) *exec.Cmd {
	return exec.Command("make", append([]string{
		"-C", b.linux,
		"O=" + b.kbuild,
		"ARCH=arm64",
		"KBUILD_BUILD_USER=swipterm",
		"KBUILD_BUILD_HOST=swipterm",
		"KBUILD_BUILD_VERSION=1",
		"KBUILD_BUILD_TIMESTAMP=" + time.Unix(b.epoch, 0).UTC().Format("Mon Jan _2 15:04:05 MST 2006"),
	}, arguments...)...)
}

func (b *builder) configureKernel() error {
	fragments, err := filepath.Glob(filepath.Join(guestPath, "kernel", "*.config"))
	if err != nil {
		return err
	}
	err = os.MkdirAll(b.kbuild, 0o755)
	if err != nil {
		return err
	}
	virtconfig := b.kernelMake("-s", "virtconfig")
	virtconfig.Stdout = io.Discard
	err = run(virtconfig)
	if err != nil {
		return err
	}
	configPath := filepath.Join(b.kbuild, ".config")
	base, err := readConfig(configPath)
	if err != nil {
		return err
	}
	merged := base
	var requested []string
	for _, fragment := range fragments {
		lines, err := readConfig(fragment)
		if err != nil {
			return err
		}
		symbols := make(map[string]bool)
		for _, line := range lines {
			if symbol := configSymbol(line); symbol != "" {
				symbols[symbol] = true
			}
		}
		merged = append(common.Filter(merged, func(line string) bool {
			return !symbols[configSymbol(line)]
		}), lines...)
		requested = append(requested, lines...)
	}
	err = os.WriteFile(configPath, []byte(strings.Join(merged, "\n")+"\n"), 0o644)
	if err != nil {
		return err
	}
	for _, target := range []string{"olddefconfig", "mod2noconfig"} {
		err = run(b.kernelMake("-s", target))
		if err != nil {
			return err
		}
	}
	resolvedLines, err := readConfig(configPath)
	if err != nil {
		return err
	}
	resolved := make(map[string]string)
	for _, line := range resolvedLines {
		if symbol := configSymbol(line); symbol != "" {
			resolved[symbol] = line
		}
	}
	mismatched := false
	for _, line := range requested {
		symbol := configSymbol(line)
		if symbol == "" {
			continue
		}
		have, exists := resolved[symbol]
		if have == line || (!exists && strings.HasPrefix(line, "#")) {
			continue
		}
		if !exists {
			have = "absent"
		}
		os.Stderr.WriteString("error: requested " + line + ", resolved " + have + "\n")
		mismatched = true
	}
	if mismatched {
		return E.New("kernel configuration does not resolve as requested")
	}
	return copyFile(configPath, filepath.Join(buildPath, "kernel.config"), 0o644)
}

func (b *builder) buildKernel() error {
	err := run(b.kernelMake("-j"+b.jobs, "Image"))
	if err != nil {
		return err
	}
	image, err := os.Open(filepath.Join(b.kbuild, "arch", "arm64", "boot", "Image"))
	if err != nil {
		return err
	}
	defer image.Close()
	return writeGzip(filepath.Join(buildPath, "Image.gz"), func(writer io.Writer) error {
		_, err := io.Copy(writer, image)
		return err
	})
}

func readConfig(path string) ([]string, error) {
	file, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer file.Close()
	var lines []string
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		lines = append(lines, scanner.Text())
	}
	return lines, scanner.Err()
}

func configSymbol(line string) string {
	match := configLine.FindStringSubmatch(line)
	if match == nil {
		return ""
	}
	return match[1] + match[2]
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

func (silentDownload) Verifying() {}
