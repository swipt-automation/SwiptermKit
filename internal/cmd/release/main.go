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
	"github.com/swipt-automation/SwiptermKit/internal/tool"
)

const manifestPath = "Package.swift"

var (
	binaryInputs = []string{
		"VMM", "Guest/swiptermd/protocol.h", "Guest/swiptermd/wire_format.h", "go.mod", "go.sum", "internal", "libswipterm",
		":(exclude)internal/cmd/build_guest", ":(exclude)internal/cmd/release",
		":(exclude)internal/tool",
	}
	binaryFrameworks = []string{"LibSwipterm"}
	versionPattern   = regexp.MustCompile(`^(\d+)\.(\d+)\.(\d+)$`)
	errUsage         = E.New("usage: release resolve [VERSION] | release manifest [-repository OWNER/NAME -binary-tag BINARY_TAG]")
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
	remote, err := tool.Output(exec.Command("git", "ls-remote", "--tags", "--refs", "origin"))
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
	var result strings.Builder
	result.WriteString(F.ToString("version=", nextVersion, "\n"))
	for _, artifact := range []struct {
		name   string
		inputs []string
	}{
		{"binary", binaryInputs},
	} {
		files, err := tool.Output(exec.Command("git", append([]string{"ls-files", "-s", "--"}, artifact.inputs...)...))
		if err != nil {
			return err
		}
		hash := sha1.New()
		hash.Write([]byte(F.ToString("blob ", len(files), "\x00", files)))
		tag := artifact.name + "." + hex.EncodeToString(hash.Sum(nil))[:12]
		result.WriteString(F.ToString(
			artifact.name, "-tag=", tag, "\n",
			artifact.name, "-exists=", exec.Command("gh", "release", "view", tag, "--json", "tagName").Run() == nil, "\n",
		))
	}
	path := os.Getenv("GITHUB_OUTPUT")
	if path == "" {
		_, err = os.Stdout.WriteString(result.String())
		return err
	}
	file, err := os.OpenFile(path, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0o644)
	if err != nil {
		return err
	}
	_, err = file.WriteString(result.String())
	return E.Errors(err, file.Close())
}

func resolveVersion(requested string, tags []string, now time.Time) (version, error) {
	versions := common.FilterNotDefault(common.Map(tags, func(tag string) version {
		parsed, _ := parseVersion(tag)
		return parsed
	}))
	var latest version
	if len(versions) > 0 {
		latest = slices.MaxFunc(versions, func(left version, right version) int {
			return slices.Compare(left[:], right[:])
		})
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
	if len(versions) > 0 && slices.Compare(next[:], latest[:]) <= 0 {
		return version{}, E.New("version must be newer than ", latest, ": ", next)
	}
	return next, nil
}

func manifest(arguments []string) error {
	flags := flag.NewFlagSet("manifest", flag.ContinueOnError)
	repository := flags.String("repository", "", "GitHub repository that publishes the binaries")
	binaryTag := flags.String("binary-tag", "", "release tag of LibSwipterm")
	err := flags.Parse(arguments)
	if err != nil {
		return err
	}
	if flags.NArg() != 0 || (*repository == "") != (*binaryTag == "") {
		return errUsage
	}
	content, err := os.ReadFile(manifestPath)
	if err != nil {
		return err
	}
	for _, release := range []struct {
		tag        string
		frameworks []string
	}{
		{*binaryTag, binaryFrameworks},
	} {
		for _, name := range release.frameworks {
			frameworkPath := "Frameworks/" + name + ".xcframework"
			location := regexp.MustCompile(`(?m)^([ \t]*)(?:path: "` + regexp.QuoteMeta(frameworkPath) + `"|url: "[^"\n]*/` +
				regexp.QuoteMeta(name) + `\.xcframework\.zip",\n[ \t]*checksum: "[^"\n]*")$`)
			if len(location.FindAllIndex(content, -1)) != 1 {
				return E.New(manifestPath, " must declare the ", name, " binary target by exactly one path or url and checksum")
			}
			replacement := `${1}path: "` + frameworkPath + `"`
			if *repository != "" {
				archive, err := os.Open("build/" + name + ".xcframework.zip")
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
					`${1}url: "https://github.com/`, *repository, "/releases/download/", release.tag, "/", name, `.xcframework.zip",`, "\n",
					`${1}checksum: "`, hex.EncodeToString(hash.Sum(nil)), `"`,
				)
			}
			content = location.ReplaceAll(content, []byte(replacement))
		}
	}
	return os.WriteFile(manifestPath, content, 0o644)
}
