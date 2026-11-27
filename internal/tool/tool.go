package tool

import (
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	E "github.com/sagernet/sing/common/exceptions"
)

func Run(command *exec.Cmd) error {
	command.Stdout = os.Stdout
	command.Stderr = os.Stderr
	err := command.Run()
	if err != nil {
		return E.Cause(err, "run ", strings.Join(command.Args, " "))
	}
	return nil
}

func Output(command *exec.Cmd) (string, error) {
	command.Stderr = os.Stderr
	data, err := command.Output()
	if err != nil {
		return "", E.Cause(err, "run ", strings.Join(command.Args, " "))
	}
	return strings.TrimSpace(string(data)), nil
}

func CopyFile(source string, destination string, mode os.FileMode) error {
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
