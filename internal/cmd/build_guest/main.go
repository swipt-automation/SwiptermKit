package main

import (
	"crypto/sha256"
	"encoding/hex"
	"io"
	"log"
	"os"
	"path/filepath"
	"strings"

	E "github.com/sagernet/sing/common/exceptions"
)

const (
	guestPath     = "Guest"
	buildPath     = guestPath + "/build"
	lockPath      = guestPath + "/artifacts.lock"
	resourcesPath = "Sources/SwiptermKit/Resources"
	toolPath      = "internal/cmd/build_guest"
)

var artifacts = []string{"Image.gz", "initramfs.cpio.gz"}

func main() {
	if len(os.Args) != 2 {
		log.Fatal("usage: build_guest all|install|check|digest|clean")
	}
	var err error
	switch os.Args[1] {
	case "all":
		err = newBuilder().build()
	case "install":
		err = newBuilder().build()
		if err == nil {
			err = install()
		}
	case "check":
		err = check()
	case "digest":
		err = writeLock()
	case "clean":
		err = os.RemoveAll(buildPath)
	default:
		err = E.New("unknown command: ", os.Args[1])
	}
	if err != nil {
		log.Fatal(err)
	}
}

func install() error {
	err := os.MkdirAll(resourcesPath, 0o755)
	if err != nil {
		return err
	}
	for _, artifact := range artifacts {
		err = copyFile(filepath.Join(buildPath, artifact), filepath.Join(resourcesPath, artifact), 0o644)
		if err != nil {
			return err
		}
	}
	return writeLock()
}

func writeLock() error {
	content, err := computeLock()
	if err != nil {
		return err
	}
	return os.WriteFile(lockPath, []byte(content), 0o644)
}

func check() error {
	content, err := computeLock()
	if err != nil {
		return err
	}
	recorded, err := os.ReadFile(lockPath)
	if err != nil {
		return err
	}
	if string(recorded) == content {
		return nil
	}
	os.Stderr.WriteString("recorded:\n" + string(recorded) + "computed:\n" + content)
	return E.New(lockPath, " does not match; run go run ./", toolPath, " install")
}

func computeLock() (string, error) {
	var inputs []string
	for _, pattern := range []string{toolPath + "/*.go", guestPath + "/kernel/*.config", guestPath + "/swiptermd/*.[ch]"} {
		matches, err := filepath.Glob(pattern)
		if err != nil {
			return "", err
		}
		inputs = append(inputs, matches...)
	}
	var list strings.Builder
	for _, input := range inputs {
		sum, err := fileSHA256(input)
		if err != nil {
			return "", err
		}
		list.WriteString(sum + "  " + filepath.ToSlash(input) + "\n")
	}
	digest := sha256.Sum256([]byte(list.String()))
	var content strings.Builder
	content.WriteString(hex.EncodeToString(digest[:]) + "  inputs\n")
	for _, artifact := range artifacts {
		sum, err := fileSHA256(filepath.Join(resourcesPath, artifact))
		if err != nil {
			return "", err
		}
		content.WriteString(sum + "  " + artifact + "\n")
	}
	return content.String(), nil
}

func fileSHA256(path string) (string, error) {
	file, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer file.Close()
	hash := sha256.New()
	_, err = io.Copy(hash, file)
	if err != nil {
		return "", E.Cause(err, "read ", path)
	}
	return hex.EncodeToString(hash.Sum(nil)), nil
}

func copyFile(source string, destination string, mode os.FileMode) error {
	input, err := os.Open(source)
	if err != nil {
		return err
	}
	defer input.Close()
	err = os.MkdirAll(filepath.Dir(destination), 0o755)
	if err != nil {
		return err
	}
	output, err := os.OpenFile(destination, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, mode)
	if err != nil {
		return err
	}
	_, err = io.Copy(output, input)
	err = E.Errors(err, output.Close())
	if err != nil {
		return err
	}
	return os.Chmod(destination, mode)
}
