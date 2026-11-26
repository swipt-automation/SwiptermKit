package main

import (
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"slices"
	"strings"

	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
)

func verify() error {
	for _, name := range frameworks {
		for _, slice := range slicesToBuild {
			framework := filepath.Join(frameworksPath, name+".xcframework", slice.identifier, name+".framework")
			weakImports, err := verifyFramework(name, framework, slice)
			if err != nil {
				return E.Cause(err, "verify ", framework)
			}
			log.Println(name, slice.identifier, "ok,", weakImports, "weak Metal imports")
		}
	}
	return nil
}

func verifyFramework(name string, framework string, slice slice) (int, error) {
	binary := filepath.Join(framework, name)
	architectures, err := output(exec.Command("lipo", "-archs", binary))
	if err != nil {
		return 0, err
	}
	if architectures != "arm64" {
		return 0, E.New("architectures: ", architectures)
	}
	buildVersion, err := output(exec.Command("vtool", "-show-build", binary))
	if err != nil {
		return 0, err
	}
	fields := make(map[string]string)
	for line := range strings.Lines(buildVersion) {
		key, value, _ := strings.Cut(strings.TrimSpace(line), " ")
		fields[key] = strings.TrimSpace(value)
	}
	if fields["platform"] != slice.vtool || fields["minos"] != iosVersion {
		return 0, E.New("build version: ", fields["platform"], " ", fields["minos"])
	}
	for key, expected := range map[string]string{"CFBundleExecutable": name, "MinimumOSVersion": iosVersion} {
		value, err := output(exec.Command("plutil", "-extract", key, "raw", filepath.Join(framework, "Info.plist")))
		if err != nil {
			return 0, err
		}
		if value != expected {
			return 0, E.New(key, ": ", value)
		}
	}
	installName, err := output(exec.Command("otool", "-D", binary))
	if err != nil {
		return 0, err
	}
	if lines := strings.Split(installName, "\n"); lines[len(lines)-1] != "@rpath/"+name+".framework/"+name {
		return 0, E.New("install name: ", lines[len(lines)-1])
	}
	exportOutput, err := output(exec.Command("nm", "-gUj", binary))
	if err != nil {
		return 0, err
	}
	exports := strings.Fields(exportOutput)
	slices.Sort(exports)
	if !slices.Equal(exports, icdExports) {
		return 0, E.New("exports: ", strings.Join(exports, " "))
	}
	libraries, err := output(exec.Command("otool", "-L", binary))
	if err != nil {
		return 0, err
	}
	if strings.Contains(libraries, "/PrivateFrameworks/") {
		return 0, E.New("links a private framework:\n", libraries)
	}
	symbols, err := output(exec.Command("nm", "-m", binary))
	if err != nil {
		return 0, err
	}
	var weakImports int
	for line := range strings.Lines(symbols) {
		line = strings.TrimSpace(line)
		if strings.Contains(line, "dynamically looked up") {
			return 0, E.New("flat namespace import: ", line)
		}
		if !strings.Contains(line, "(undefined)") || !strings.HasSuffix(line, "(from Metal)") {
			continue
		}
		if strings.Contains(line, "(undefined) weak external") {
			weakImports++
		} else if symbol := strings.Fields(line); metal4Class.MatchString(symbol[len(symbol)-3]) {
			return 0, E.New("Metal 4 class is not weakly imported: ", line)
		}
	}
	signed, err := os.MkdirTemp("", "build_vulkan-")
	if err != nil {
		return 0, err
	}
	defer os.RemoveAll(signed)
	signedFramework := filepath.Join(signed, name+".framework")
	err = os.CopyFS(signedFramework, os.DirFS(framework))
	if err != nil {
		return 0, err
	}
	err = common.Error(output(exec.Command("codesign", "--force", "--sign", "-", signedFramework)))
	if err == nil {
		err = common.Error(output(exec.Command("codesign", "--verify", "--strict", signedFramework)))
	}
	return weakImports, err
}
