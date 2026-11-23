package main

import (
	"crypto/sha1"
	"crypto/sha256"
	"encoding/hex"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"regexp"
	"slices"
	"strconv"
	"strings"
	"time"

	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	F "github.com/sagernet/sing/common/format"
)

const (
	manifestPath  = "Package.swift"
	archivePath   = "build/LibSwipterm.xcframework.zip"
	frameworkPath = "Frameworks/LibSwipterm.xcframework"
)

var (
	binaryInputs = []string{
		"VMM", "Guest/swiptermd/protocol.h", "Guest/swiptermd/wire_format.h", "go.mod", "go.sum", "internal", "libswipterm",
		":(exclude)internal/cmd/build_guest", ":(exclude)internal/cmd/release",
	}
	versionPattern  = regexp.MustCompile(`^(\d+)\.(\d+)\.(\d+)$`)
	binaryLocation  = regexp.MustCompile(`(?m)^([ \t]*)(?:path: "` + regexp.QuoteMeta(frameworkPath) + `"|url: "[^"\n]*",\n[ \t]*checksum: "[^"\n]*")$`)
	errUsage        = E.New("usage: release resolve [VERSION] | release manifest [-repository OWNER/NAME -tag BINARY_TAG]")
	errMissingMatch = E.New(manifestPath, " must declare the LibSwipterm binary target by exactly one path or url and checksum")
)

type version [3]int64

func parseVersion(value string) (version, bool) {
	match := versionPattern.FindStringSubmatch(value)
	if match == nil {
		return version{}, false
	}
	var parsed version
	for index := range parsed {
		number, err := strconv.ParseInt(match[index+1], 10, 64)
		if err != nil {
			return version{}, false
		}
		parsed[index] = number
	}
	return parsed, true
}

func (v version) String() string {
	return F.ToString(v[0], ".", v[1], ".", v[2])
}

func main() {
	if len(os.Args) < 2 {
		log.Fatal(errUsage)
	}
	var err error
	switch os.Args[1] {
	case "resolve":
		err = resolve(os.Args[2:])
	case "manifest":
		err = manifest(os.Args[2:])
	default:
		err = errUsage
	}
	if err != nil {
		log.Fatal(err)
	}
}

func resolve(arguments []string) error {
	if len(arguments) > 1 {
		return errUsage
	}
	requested := ""
	if len(arguments) == 1 {
		requested = arguments[0]
	}
	remote, err := output("git", "ls-remote", "--tags", "--refs", "origin")
	if err != nil {
		return err
	}
	tags := common.Map(strings.Fields(remote), func(field string) string {
		return strings.TrimPrefix(field, "refs/tags/")
	})
	nextVersion, err := resolveVersion(requested, tags, time.Now().UTC())
	if err != nil {
		return err
	}
	files, err := output("git", append([]string{"ls-files", "-s", "--"}, binaryInputs...)...)
	if err != nil {
		return err
	}
	hash := sha1.New()
	hash.Write([]byte(F.ToString("blob ", len(files), "\x00", files)))
	binaryTag := "binary." + hex.EncodeToString(hash.Sum(nil))[:12]
	result := F.ToString(
		"version=", nextVersion, "\n",
		"binary-tag=", binaryTag, "\n",
		"binary-exists=", exec.Command("gh", "release", "view", binaryTag, "--json", "tagName").Run() == nil, "\n",
	)
	path := os.Getenv("GITHUB_OUTPUT")
	if path == "" {
		_, err = os.Stdout.WriteString(result)
		return err
	}
	file, err := os.OpenFile(path, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0o644)
	if err != nil {
		return err
	}
	_, err = file.WriteString(result)
	return E.Errors(err, file.Close())
}

func resolveVersion(requested string, tags []string, now time.Time) (version, error) {
	versions := common.FilterNotDefault(common.Map(tags, func(tag string) version {
		parsed, _ := parseVersion(tag)
		return parsed
	}))
	var latest version
	if len(versions) > 0 {
		latest = slices.MaxFunc(versions, compareVersions)
	}
	var next version
	if requested != "" {
		parsed, ok := parseVersion(requested)
		if !ok {
			return version{}, E.New("version must be MAJOR.MINOR.PATCH: ", requested)
		}
		next = parsed
	} else {
		if len(versions) == 0 {
			return version{}, E.New("no version tag to derive MAJOR.MINOR from; pass a version")
		}
		today := now.Format("20060102")
		revision := int64(1)
		if patch := strconv.FormatInt(latest[2], 10); strings.HasPrefix(patch, today) && len(patch) > len(today) {
			previous, err := strconv.ParseInt(patch[len(today):], 10, 64)
			if err != nil {
				return version{}, err
			}
			revision = previous + 1
		}
		patch, err := strconv.ParseInt(fmt.Sprintf("%s%02d", today, revision), 10, 64)
		if err != nil {
			return version{}, err
		}
		next = version{latest[0], latest[1], patch}
	}
	if len(versions) > 0 && compareVersions(next, latest) <= 0 {
		return version{}, E.New("version must be newer than ", latest, ": ", next)
	}
	return next, nil
}

func compareVersions(left version, right version) int {
	return slices.Compare(left[:], right[:])
}

func manifest(arguments []string) error {
	flags := flag.NewFlagSet("manifest", flag.ContinueOnError)
	repository := flags.String("repository", "", "GitHub repository that publishes the binary")
	tag := flags.String("tag", "", "release tag of the binary")
	err := flags.Parse(arguments)
	if err != nil {
		return err
	}
	if flags.NArg() != 0 || (*repository == "") != (*tag == "") {
		return errUsage
	}
	content, err := os.ReadFile(manifestPath)
	if err != nil {
		return err
	}
	if len(binaryLocation.FindAllIndex(content, -1)) != 1 {
		return errMissingMatch
	}
	replacement := `${1}path: "` + frameworkPath + `"`
	if *repository != "" {
		archive, err := os.Open(archivePath)
		if err != nil {
			return err
		}
		hash := sha256.New()
		_, err = io.Copy(hash, archive)
		archive.Close()
		if err != nil {
			return err
		}
		replacement = F.ToString(
			`${1}url: "https://github.com/`, *repository, "/releases/download/", *tag, `/LibSwipterm.xcframework.zip",`, "\n",
			`${1}checksum: "`, hex.EncodeToString(hash.Sum(nil)), `"`,
		)
	}
	return os.WriteFile(manifestPath, binaryLocation.ReplaceAll(content, []byte(replacement)), 0o644)
}

func output(name string, arguments ...string) (string, error) {
	command := exec.Command(name, arguments...)
	command.Stderr = os.Stderr
	data, err := command.Output()
	if err != nil {
		return "", E.Cause(err, "run ", strings.Join(command.Args, " "))
	}
	return string(data), nil
}
