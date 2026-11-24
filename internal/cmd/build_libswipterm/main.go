package main

import (
	"bytes"
	"debug/buildinfo"
	"go/build"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"runtime/debug"
	"slices"
	"strings"
	"text/template"

	_ "github.com/sagernet/gomobile"
	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"

	"golang.org/x/sync/errgroup"
)

const (
	gomobileModule = "github.com/sagernet/gomobile"
	iosVersion     = "17.0"
	macOSVersion   = "14.0"
	frameworkName  = "LibSwipterm"
	buildPath      = "build"
	gomobilePath   = buildPath + "/gomobile/" + frameworkName + ".xcframework"
	slicesPath     = buildPath + "/frameworks"
	outputPath     = "Frameworks/" + frameworkName + ".xcframework"
	archivePath    = buildPath + "/" + frameworkName + ".xcframework.zip"
)

type platform struct {
	sdk               string
	target            string
	slice             string
	bundlePlatform    string
	minimumVersionKey string
	minimumVersion    string
	versioned         bool
}

var platforms = []platform{
	{
		sdk:               "macosx",
		target:            "arm64-apple-macos" + macOSVersion,
		slice:             "macos-arm64",
		bundlePlatform:    "MacOSX",
		minimumVersionKey: "LSMinimumSystemVersion",
		minimumVersion:    macOSVersion,
		versioned:         true,
	},
	{
		sdk:               "iphoneos",
		target:            "arm64-apple-ios" + iosVersion,
		slice:             "ios-arm64",
		bundlePlatform:    "iPhoneOS",
		minimumVersionKey: "MinimumOSVersion",
		minimumVersion:    iosVersion,
	},
	{
		sdk:               "iphonesimulator",
		target:            "arm64-apple-ios" + iosVersion + "-simulator",
		slice:             "ios-arm64-simulator",
		bundlePlatform:    "iPhoneSimulator",
		minimumVersionKey: "MinimumOSVersion",
		minimumVersion:    iosVersion,
	},
}

var infoPlist = template.Must(template.New("Info.plist").Parse(`<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>en</string>
	<key>CFBundleExecutable</key>
	<string>{{.Name}}</string>
	<key>CFBundleIdentifier</key>
	<string>io.swipt.{{.Name}}</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>{{.Name}}</string>
	<key>CFBundlePackageType</key>
	<string>FMWK</string>
	<key>CFBundleShortVersionString</key>
	<string>1.0</string>
	<key>CFBundleSupportedPlatforms</key>
	<array>
		<string>{{.Platform}}</string>
	</array>
	<key>CFBundleVersion</key>
	<string>1</string>
	<key>{{.MinimumVersionKey}}</key>
	<string>{{.MinimumVersion}}</string>
</dict>
</plist>
`))

func main() {
	err := installGomobile()
	if err == nil {
		err = buildLibraries()
	}
	if err == nil {
		err = bind()
	}
	if err == nil {
		err = assemble()
	}
	if err != nil {
		log.Fatal(err)
	}
}

func installGomobile() error {
	info, loaded := debug.ReadBuildInfo()
	if !loaded {
		return E.New("missing build info")
	}
	module := common.Find(info.Deps, func(module *debug.Module) bool {
		return module.Path == gomobileModule
	})
	if module == nil {
		return E.New("missing ", gomobileModule, " in build info")
	}
	binDirectory := gomobileBinDirectory()
	var missingTools []string
	for _, tool := range []string{"gomobile", "gobind"} {
		info, err := buildinfo.ReadFile(filepath.Join(binDirectory, tool))
		if err != nil || info.Path != gomobileModule+"/cmd/"+tool || info.Main.Path != gomobileModule || info.Main.Version != module.Version {
			missingTools = append(missingTools, tool)
		}
	}
	if len(missingTools) == 0 {
		return nil
	}
	install := exec.Command("go", append([]string{"install", "-v"}, common.Map(missingTools, func(tool string) string {
		return gomobileModule + "/cmd/" + tool + "@" + module.Version
	})...)...)
	install.Env = append(os.Environ(), "GOBIN="+binDirectory)
	return run(install)
}

func gomobileBinDirectory() string {
	return filepath.Join(filepath.SplitList(build.Default.GOPATH)[0], "bin")
}

func buildLibraries() error {
	sources, err := glob("VMM/board/*.c", "VMM/cpu/*.c", "VMM/host/*.c", "VMM/mmu/*.c", "VMM/virtio/*.c", "VMM/cpu/cpu_fast_arm64.S")
	if err != nil {
		return err
	}
	zstdSources, err := glob("VMM/deps/zstd/lib/common/*.c", "VMM/deps/zstd/lib/decompress/*.c")
	if err != nil {
		return err
	}
	xzSources, err := glob("VMM/deps/xz/src/common/*.c", "VMM/deps/xz/src/liblzma/*/*.c")
	if err != nil {
		return err
	}
	clang, err := output(exec.Command("xcrun", "--find", "clang"))
	if err != nil {
		return err
	}
	libtool, err := output(exec.Command("xcrun", "--find", "libtool"))
	if err != nil {
		return err
	}
	statistics := "-UCPU_STATS"
	if os.Getenv("SWIPTERM_CPU_STATS") == "1" {
		statistics = "-DCPU_STATS"
	}
	var group errgroup.Group
	group.SetLimit(runtime.NumCPU())
	objects := make([][]string, len(platforms))
	flagFiles := make([]string, len(platforms))
	flags := make([]string, len(platforms))
	for index, platform := range platforms {
		sdkPath, err := output(exec.Command("xcrun", "--sdk", platform.sdk, "--show-sdk-path"))
		if err != nil {
			return err
		}
		base := []string{
			"-target", platform.target, "-isysroot", sdkPath, "-O3", "-g", "-fno-common",
			"-IVMM/include/CSwiptermKit", "-IVMM/deps/zstd/lib", "-IVMM/deps/zstd/lib/common", "-IVMM/deps/xz/src/liblzma/api",
			"-DZSTD_LEGACY_SUPPORT=0", "-DXXH_NAMESPACE=swipterm_zstd_", statistics,
		}
		vmmFlags := append(slices.Clone(base), "-std=gnu2x", "-Wall", "-Wextra", "-Werror")
		zstdFlags := append(slices.Clone(base), "-std=gnu11", "-w")
		xzFlags := append(slices.Clone(base), "-std=gnu11", "-w", "-fvisibility=hidden", "-DHAVE_CONFIG_H", "-DNDEBUG",
			"-IVMM/deps/xz", "-IVMM/deps/xz/src/common", "-IVMM/deps/xz/src/liblzma/common", "-IVMM/deps/xz/src/liblzma/check",
			"-IVMM/deps/xz/src/liblzma/lz", "-IVMM/deps/xz/src/liblzma/rangecoder", "-IVMM/deps/xz/src/liblzma/lzma",
			"-IVMM/deps/xz/src/liblzma/delta", "-IVMM/deps/xz/src/liblzma/simple",
		)
		flags[index] = strings.Join(slices.Concat(vmmFlags, zstdFlags, xzFlags), "\n")
		flagFiles[index] = filepath.Join(buildPath, platform.sdk, "flags")
		previous, _ := os.ReadFile(flagFiles[index])
		rebuild := string(previous) != flags[index]
		for _, unit := range []struct{ sources, arguments []string }{
			{sources, vmmFlags},
			{zstdSources, zstdFlags},
			{xzSources, xzFlags},
		} {
			for _, source := range unit.sources {
				object := filepath.Join(buildPath, platform.sdk, source+".o")
				objects[index] = append(objects[index], object)
				if !rebuild && upToDate(object) {
					continue
				}
				group.Go(func() error {
					err := os.MkdirAll(filepath.Dir(object), 0o755)
					if err != nil {
						return err
					}
					return run(exec.Command(clang, append(slices.Clone(unit.arguments), "-MMD", "-MP", "-c", source, "-o", object)...))
				})
			}
		}
	}
	err = group.Wait()
	if err != nil {
		return err
	}
	for index, platform := range platforms {
		library := filepath.Join(buildPath, platform.sdk, "libvmm.a")
		os.Remove(library)
		err = run(exec.Command(libtool, append([]string{"-static", "-no_warning_for_no_symbols", "-o", library}, objects[index]...)...))
		if err != nil {
			return err
		}
		err = os.WriteFile(flagFiles[index], []byte(flags[index]), 0o644)
		if err != nil {
			return err
		}
	}
	return nil
}

func glob(patterns ...string) ([]string, error) {
	var matches []string
	for _, pattern := range patterns {
		patternMatches, err := filepath.Glob(pattern)
		if err != nil {
			return nil, err
		}
		matches = append(matches, patternMatches...)
	}
	return matches, nil
}

func upToDate(object string) bool {
	objectInfo, err := os.Stat(object)
	if err != nil {
		return false
	}
	dependencies, err := os.ReadFile(strings.TrimSuffix(object, ".o") + ".d")
	if err != nil {
		return false
	}
	rule, _, _ := strings.Cut(strings.ReplaceAll(string(dependencies), "\\\n", " "), "\n")
	_, prerequisites, found := strings.Cut(rule, ":")
	if !found {
		return false
	}
	for path := range strings.FieldsSeq(prerequisites) {
		info, err := os.Stat(path)
		if err != nil || info.ModTime().After(objectInfo.ModTime()) {
			return false
		}
	}
	return true
}

func bind() error {
	err := os.RemoveAll(gomobilePath)
	if err == nil {
		err = os.MkdirAll(filepath.Dir(gomobilePath), 0o755)
	}
	if err != nil {
		return err
	}
	binDirectory := gomobileBinDirectory()
	command := exec.Command(filepath.Join(binDirectory, "gomobile"),
		"bind",
		"-v",
		"-target", "ios/arm64,iossimulator/arm64,macos/arm64",
		"-iosversion", iosVersion,
		"-macosversion", macOSVersion,
		"-trimpath",
		"-buildvcs=false",
		"-ldflags", "-s -w -buildid= -checklinkname=0",
		"-o", gomobilePath,
		"./libswipterm",
	)
	command.Env = append(os.Environ(), "PATH="+binDirectory+string(os.PathListSeparator)+os.Getenv("PATH"))
	return run(command)
}

func assemble() error {
	libtool, err := output(exec.Command("xcrun", "--find", "libtool"))
	if err != nil {
		return err
	}
	err = os.RemoveAll(slicesPath)
	if err != nil {
		return err
	}
	createArguments := []string{"-create-xcframework"}
	for _, platform := range platforms {
		source := filepath.Join(gomobilePath, platform.slice, frameworkName+".framework", "Versions", "A")
		framework, err := filepath.Abs(filepath.Join(slicesPath, platform.slice, frameworkName+".framework"))
		if err != nil {
			return err
		}
		contents := framework
		resources := framework
		if platform.versioned {
			contents = filepath.Join(framework, "Versions", "A")
			resources = filepath.Join(contents, "Resources")
		}
		err = os.MkdirAll(resources, 0o755)
		if err != nil {
			return err
		}
		err = run(exec.Command(libtool, "-static", "-no_warning_for_no_symbols", "-o", filepath.Join(contents, frameworkName),
			filepath.Join(source, frameworkName), filepath.Join(buildPath, platform.sdk, "libvmm.a")))
		if err != nil {
			return err
		}
		for _, directory := range []string{"Headers", "Modules"} {
			err = os.CopyFS(filepath.Join(contents, directory), os.DirFS(filepath.Join(source, directory)))
			if err != nil {
				return err
			}
		}
		var plist bytes.Buffer
		err = infoPlist.Execute(&plist, map[string]string{
			"Name":              frameworkName,
			"Platform":          platform.bundlePlatform,
			"MinimumVersionKey": platform.minimumVersionKey,
			"MinimumVersion":    platform.minimumVersion,
		})
		if err != nil {
			return err
		}
		err = os.WriteFile(filepath.Join(resources, "Info.plist"), plist.Bytes(), 0o644)
		if err != nil {
			return err
		}
		if platform.versioned {
			err = os.Symlink("A", filepath.Join(framework, "Versions", "Current"))
			if err != nil {
				return err
			}
			for _, name := range []string{frameworkName, "Headers", "Modules", "Resources"} {
				err = os.Symlink(filepath.Join("Versions", "Current", name), filepath.Join(framework, name))
				if err != nil {
					return err
				}
			}
		}
		createArguments = append(createArguments, "-framework", framework)
	}
	err = os.RemoveAll(outputPath)
	if err != nil {
		return err
	}
	err = run(exec.Command("xcodebuild", append(createArguments, "-output", outputPath)...))
	if err != nil {
		return err
	}
	err = os.RemoveAll(archivePath)
	if err != nil {
		return err
	}
	return run(exec.Command("ditto", "-c", "-k", "--norsrc", "--noextattr", "--keepParent", outputPath, archivePath))
}

func output(command *exec.Cmd) (string, error) {
	command.Stderr = os.Stderr
	data, err := command.Output()
	if err != nil {
		return "", E.Cause(err, "run ", strings.Join(command.Args, " "))
	}
	return strings.TrimSpace(string(data)), nil
}

func run(command *exec.Cmd) error {
	command.Stdout = os.Stdout
	command.Stderr = os.Stderr
	err := command.Run()
	if err != nil {
		return E.Cause(err, "run ", strings.Join(command.Args, " "))
	}
	return nil
}
