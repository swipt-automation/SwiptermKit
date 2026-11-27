package main

import (
	"bytes"
	"embed"
	"errors"
	"io/fs"
	"log"
	"maps"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"regexp"
	"slices"
	"strings"
	"text/template"

	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/swipt-automation/SwiptermKit/internal/tool"
)

const (
	iosVersion     = "17.0"
	frameworksPath = "Frameworks"
	buildPath      = "build"
	kosmicKrisp    = "KosmicKrisp"
	moltenVK       = "MoltenVK"
	llvmFormula    = "llvm@23"
)

type repository struct {
	directory string
	url       string
	commit    string
}

var (
	mesa                = repository{"mesa", "https://gitlab.freedesktop.org/mesa/mesa.git", "b8c965f2d4164fa33e79539827ed2bac0a0e2851"}
	moltenVKSource      = repository{"MoltenVK", "https://github.com/KhronosGroup/MoltenVK.git", "5189f8de86ec365860f63bf1da736815cfaea5cd"}
	spirvHeaders        = repository{"SPIRV-Headers", "https://github.com/KhronosGroup/SPIRV-Headers.git", "5c6004e758e0c80185ef2cee93ae5c10fa342a5b"}
	spirvTools          = repository{"SPIRV-Tools", "https://github.com/KhronosGroup/SPIRV-Tools.git", "ef96ed763b43b59b33b31b362f09a02b729fa1c9"}
	spirvLLVMTranslator = repository{"SPIRV-LLVM-Translator", "https://github.com/KhronosGroup/SPIRV-LLVM-Translator.git", "b5b8fce1604ee82ab8f5bced7e7c7961b5bd4de4"}

	hostTools    = []string{"src/compiler/clc/mesa_clc", "src/compiler/spirv/vtn_bindgen2", "src/kosmickrisp/clc/kk_clc"}
	icdExports   = []string{"_vk_icdGetInstanceProcAddr", "_vk_icdGetPhysicalDeviceProcAddr", "_vk_icdNegotiateLoaderICDInterfaceVersion"}
	frameworks   = []string{kosmicKrisp, moltenVK}
	nullableName = regexp.MustCompile(`^_[a-z][a-z0-9_]*_[A-Z][A-Za-z0-9]*$`)
	metal4Class  = regexp.MustCompile(`^_OBJC_CLASS_\$_(MTL4|MTLResidencySet)`)
)

type slice struct {
	sdk        string
	subsystem  string
	triple     string
	platform   string
	identifier string
	vtool      string
}

var slicesToBuild = []slice{
	{"iphoneos", "ios", "arm64-apple-ios" + iosVersion, "iPhoneOS", "ios-arm64", "IOS"},
	{"iphonesimulator", "ios-simulator", "arm64-apple-ios" + iosVersion + "-simulator", "iPhoneSimulator", "ios-arm64-simulator", "IOSSIMULATOR"},
}

//go:embed patches
var patches embed.FS

var crossFile = template.Must(template.New("cross.ini").Parse(`[binaries]
c = '{{.CC}}'
cpp = '{{.CXX}}'
objc = '{{.CC}}'
objcpp = '{{.CXX}}'
ar = '{{.AR}}'
strip = '{{.Strip}}'

[built-in options]
c_args = [{{.Flags}}]
cpp_args = [{{.Flags}}]
objc_args = [{{.Flags}}]
objcpp_args = [{{.Flags}}]
c_link_args = [{{.Flags}}]
cpp_link_args = [{{.Flags}}]
objc_link_args = [{{.Flags}}]
objcpp_link_args = [{{.Flags}}]

[properties]
needs_exe_wrapper = true

[host_machine]
system = 'darwin'
subsystem = '{{.Subsystem}}'
kernel = 'xnu'
cpu_family = 'aarch64'
cpu = 'arm64'
endian = 'little'
`))

var infoPlist = template.Must(template.New("Info.plist").Parse(`<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>en</string>
	<key>CFBundleExecutable</key>
	<string>{{.Name}}</string>
	<key>CFBundleIdentifier</key>
	<string>org.freedesktop.mesa.{{.Name}}</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>{{.Name}}</string>
	<key>CFBundlePackageType</key>
	<string>FMWK</string>
	<key>CFBundleShortVersionString</key>
	<string>{{.Version}}</string>
	<key>CFBundleVersion</key>
	<string>{{.Version}}</string>
	<key>CFBundleSupportedPlatforms</key>
	<array>
		<string>{{.Platform}}</string>
	</array>
	<key>MinimumOSVersion</key>
	<string>{{.MinimumVersion}}</string>
	<key>UIDeviceFamily</key>
	<array>
		<integer>1</integer>
		<integer>2</integer>
	</array>
</dict>
</plist>
`))

type builder struct {
	source string
	build  string
	host   string
	venv   string
}

func main() {
	steps := os.Args[1:]
	if len(steps) == 0 || slices.Equal(steps, []string{"all"}) {
		steps = []string{"host-tools", "kosmickrisp", "moltenvk", "verify"}
	}
	b, err := newBuilder()
	if err != nil {
		log.Fatal(err)
	}
	for _, step := range steps {
		switch step {
		case "host-tools":
			err = b.buildHostTools()
		case "kosmickrisp":
			err = b.buildKosmicKrisp()
		case "moltenvk":
			err = b.buildMoltenVK()
		case "verify":
			err = verify()
		default:
			err = E.New("usage: build_vulkan [all | host-tools | kosmickrisp | moltenvk | verify]...")
		}
		if err != nil {
			log.Fatal(err)
		}
	}
}

func newBuilder() (*builder, error) {
	work, err := filepath.Abs(filepath.Join(buildPath, "vulkan"))
	if err != nil {
		return nil, err
	}
	b := &builder{
		source: filepath.Join(work, "src"),
		build:  filepath.Join(work, "build"),
		host:   filepath.Join(work, "host"),
		venv:   filepath.Join(work, "venv"),
	}
	err = os.MkdirAll(b.build, 0o755)
	if err != nil {
		return nil, err
	}
	return b, os.Setenv("PATH", strings.Join([]string{filepath.Join(b.venv, "bin"), filepath.Join(b.host, "bin"), os.Getenv("PATH")}, string(os.PathListSeparator)))
}

func (b *builder) buildHostTools() error {
	err := b.pythonEnvironment()
	if err != nil {
		return err
	}
	llvmPrefix := os.Getenv("LLVM_PREFIX")
	if llvmPrefix == "" {
		llvmPrefix, err = tool.Output(exec.Command("brew", "--prefix", llvmFormula))
		if err != nil {
			return err
		}
	}
	for _, repo := range []repository{spirvHeaders, spirvTools, spirvLLVMTranslator} {
		err = b.checkout(repo)
		if err != nil {
			return err
		}
	}
	headers := filepath.Join(b.source, spirvHeaders.directory)
	for _, project := range []struct {
		repo      repository
		arguments []string
	}{
		{spirvHeaders, nil},
		{spirvTools, []string{
			"-DSPIRV-Headers_SOURCE_DIR=" + headers, "-DSPIRV_SKIP_TESTS=ON", "-DSPIRV_SKIP_EXECUTABLES=ON",
			"-DSPIRV_WERROR=OFF", "-DBUILD_SHARED_LIBS=OFF",
		}},
		{spirvLLVMTranslator, []string{
			"-DLLVM_DIR=" + filepath.Join(llvmPrefix, "lib/cmake/llvm"), "-DLLVM_EXTERNAL_SPIRV_HEADERS_SOURCE_DIR=" + headers,
			"-DLLVM_SPIRV_INCLUDE_TESTS=OFF", "-DBUILD_SHARED_LIBS=OFF",
		}},
	} {
		directory := filepath.Join(b.build, "host-"+project.repo.directory)
		err = tool.Run(exec.Command("cmake", append([]string{
			"-S", filepath.Join(b.source, project.repo.directory), "-B", directory, "-G", "Ninja",
			"-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=" + b.host, "-DCMAKE_OSX_ARCHITECTURES=arm64",
		}, project.arguments...)...))
		if err == nil {
			err = tool.Run(exec.Command("cmake", "--build", directory, "--target", "install"))
		}
		if err != nil {
			return err
		}
	}
	err = b.checkout(mesa)
	if err != nil {
		return err
	}
	nativeFile := filepath.Join(b.build, "host-native.ini")
	err = os.WriteFile(nativeFile, []byte("[binaries]\nllvm-config = '"+filepath.Join(llvmPrefix, "bin/llvm-config")+"'\n"), 0o644)
	if err != nil {
		return err
	}
	mesaBuild := filepath.Join(b.build, "mesa-host")
	err = os.RemoveAll(mesaBuild)
	if err != nil {
		return err
	}
	setup := exec.Command("meson", "setup", mesaBuild, filepath.Join(b.source, mesa.directory),
		"--native-file", nativeFile, "--prefix", b.host, "--buildtype=release",
		"-Dplatforms=macos", "-Dvulkan-drivers=kosmickrisp", "-Dgallium-drivers=", "-Dopengl=false", "-Dzstd=disabled",
		"-Dllvm=enabled", "-Dshared-llvm=enabled", "-Dspirv-tools=enabled", "-Dmesa-clc=enabled", "-Dinstall-mesa-clc=true",
		"-Dprecomp-compiler=enabled", "-Dinstall-precomp-compiler=true",
	)
	setup.Env = append(os.Environ(), "PKG_CONFIG_PATH="+filepath.Join(b.host, "lib/pkgconfig"))
	err = tool.Run(setup)
	if err != nil {
		return err
	}
	err = tool.Run(exec.Command("ninja", append([]string{"-C", mesaBuild}, hostTools...)...))
	if err != nil {
		return err
	}
	for _, target := range hostTools {
		err = tool.CopyFile(filepath.Join(mesaBuild, target), filepath.Join(b.host, "bin", path.Base(target)), 0o755)
		if err != nil {
			return err
		}
	}
	return nil
}

func (b *builder) buildKosmicKrisp() error {
	for _, target := range hostTools {
		_, err := os.Stat(filepath.Join(b.host, "bin", path.Base(target)))
		if err != nil {
			return E.Cause(err, "missing ", path.Base(target), ", run build_vulkan host-tools")
		}
	}
	err := b.pythonEnvironment()
	if err == nil {
		err = b.checkout(mesa)
	}
	if err != nil {
		return err
	}
	mesaSource := filepath.Join(b.source, mesa.directory)
	versionContent, err := os.ReadFile(filepath.Join(mesaSource, "VERSION"))
	if err != nil {
		return err
	}
	version, _, _ := strings.Cut(strings.TrimSpace(string(versionContent)), "-")
	var createArguments []string
	for _, slice := range slicesToBuild {
		framework, err := b.buildKosmicKrispSlice(slice, mesaSource, version)
		if err != nil {
			return err
		}
		createArguments = append(createArguments, "-framework", framework)
	}
	return createXCFramework(kosmicKrisp, createArguments)
}

func (b *builder) buildKosmicKrispSlice(slice slice, mesaSource string, version string) (string, error) {
	tools := make(map[string]string)
	for _, name := range []string{"clang", "clang++", "ar", "strip"} {
		toolPath, err := tool.Output(exec.Command("xcrun", "--sdk", slice.sdk, "--find", name))
		if err != nil {
			return "", err
		}
		tools[name] = toolPath
	}
	sysroot, err := tool.Output(exec.Command("xcrun", "--sdk", slice.sdk, "--show-sdk-path"))
	if err != nil {
		return "", err
	}
	var cross bytes.Buffer
	err = crossFile.Execute(&cross, map[string]string{
		"CC":        tools["clang"],
		"CXX":       tools["clang++"],
		"AR":        tools["ar"],
		"Strip":     tools["strip"],
		"Flags":     "'-target', '" + slice.triple + "', '-isysroot', '" + sysroot + "'",
		"Subsystem": slice.subsystem,
	})
	if err != nil {
		return "", err
	}
	crossPath := filepath.Join(b.build, "cross-"+slice.sdk+".ini")
	err = os.WriteFile(crossPath, cross.Bytes(), 0o644)
	if err != nil {
		return "", err
	}
	mesaBuild := filepath.Join(b.build, "mesa-"+slice.sdk)
	err = os.RemoveAll(mesaBuild)
	if err == nil {
		err = tool.Run(exec.Command("meson", "setup", mesaBuild, mesaSource, "--cross-file", crossPath,
			"--buildtype=release", "-Db_ndebug=true", "-Dplatforms=", "-Dvulkan-drivers=kosmickrisp", "-Dgallium-drivers=",
			"-Dvulkan-layers=", "-Dtools=", "-Dopengl=false", "-Degl=disabled", "-Dglx=disabled", "-Dgles1=disabled",
			"-Dgles2=disabled", "-Dgbm=disabled", "-Dllvm=disabled", "-Dspirv-tools=disabled", "-Dmesa-clc=system",
			"-Dprecomp-compiler=system", "-Dxmlconfig=disabled", "-Dexpat=disabled", "-Dzstd=disabled",
			"-Dvalgrind=disabled", "-Dlibunwind=disabled", "-Dshader-cache=enabled", "-Dshader-cache-default=false",
			"-Dbuild-tests=false",
		))
	}
	if err != nil {
		return "", err
	}
	const dylib = "src/kosmickrisp/vulkan/libvulkan_kosmickrisp.dylib"
	query, err := tool.Output(exec.Command("ninja", "-C", mesaBuild, "-t", "query", dylib))
	if err != nil {
		return "", err
	}
	var archives []string
	inputs := false
	for line := range strings.Lines(query) {
		switch {
		case strings.HasPrefix(line, "  input:"):
			inputs = true
		case strings.HasPrefix(line, "  outputs:"):
			inputs = false
		case inputs:
			fields := strings.Fields(line)
			if len(fields) > 0 && strings.HasSuffix(fields[len(fields)-1], ".a") {
				archives = append(archives, fields[len(fields)-1])
			}
		}
	}
	err = tool.Run(exec.Command("ninja", append([]string{"-C", mesaBuild}, archives...)...))
	if err != nil {
		return "", err
	}
	nullEntrypoints := filepath.Join(mesaBuild, "src/kosmickrisp/vulkan/kk_null_entrypoints")
	err = writeNullEntrypoints(mesaBuild, nullEntrypoints+".s", archives)
	if err == nil {
		err = tool.Run(exec.Command(tools["clang"], "-target", slice.triple, "-isysroot", sysroot, "-c", nullEntrypoints+".s", "-o", nullEntrypoints+".o"))
	}
	if err == nil {
		err = tool.Run(exec.Command("ninja", "-C", mesaBuild, dylib))
	}
	if err != nil {
		return "", err
	}
	framework := filepath.Join(mesaBuild, "framework", kosmicKrisp+".framework")
	binary := filepath.Join(framework, kosmicKrisp)
	err = tool.CopyFile(filepath.Join(mesaBuild, dylib), binary, 0o755)
	if err == nil {
		err = tool.Run(exec.Command("strip", "-x", binary))
	}
	if err == nil {
		err = tool.CopyFile(filepath.Join(mesaSource, "docs/license.rst"), filepath.Join(framework, "LICENSE.rst"), 0o644)
	}
	if err != nil {
		return "", err
	}
	var plist bytes.Buffer
	err = infoPlist.Execute(&plist, map[string]string{
		"Name":           kosmicKrisp,
		"Version":        version,
		"Platform":       slice.platform,
		"MinimumVersion": iosVersion,
	})
	if err == nil {
		err = os.WriteFile(filepath.Join(framework, "Info.plist"), plist.Bytes(), 0o644)
	}
	return framework, err
}

func writeNullEntrypoints(directory string, outputPath string, archives []string) error {
	command := exec.Command("nm", append([]string{"-m"}, archives...)...)
	command.Dir = directory
	symbols, err := tool.Output(command)
	if err != nil {
		return err
	}
	defined := make(map[string]bool)
	weakUndefined := make(map[string]bool)
	for line := range strings.Lines(symbols) {
		line = strings.TrimSpace(line)
		name := line[strings.LastIndexByte(line, ' ')+1:]
		switch {
		case strings.Contains(line, "(undefined) weak external"):
			weakUndefined[name] = true
		case !strings.Contains(line, "(undefined)") && strings.Contains(line, " external "):
			defined[name] = true
		}
	}
	missing := common.Filter(slices.Sorted(maps.Keys(weakUndefined)), func(name string) bool {
		return !defined[name] && nullableName.MatchString(name)
	})
	var assembly strings.Builder
	for _, name := range missing {
		assembly.WriteString(".globl " + name + "\n.private_extern " + name + "\n" + name + " = 0\n")
	}
	log.Println(len(missing), "unimplemented entrypoints defined as NULL")
	return os.WriteFile(outputPath, []byte(assembly.String()), 0o644)
}

func (b *builder) buildMoltenVK() error {
	err := b.checkout(moltenVKSource)
	if err != nil {
		return err
	}
	moltenVKSourcePath := filepath.Join(b.source, moltenVKSource.directory)
	fetch := exec.Command("./fetchDependencies", "--ios", "--iossim")
	fetch.Dir = moltenVKSourcePath
	err = tool.Run(fetch)
	if err != nil {
		return err
	}
	exportsPath := filepath.Join(b.build, "vk_icd.exp")
	err = os.WriteFile(exportsPath, []byte(strings.Join(icdExports, "\n")+"\n"), 0o644)
	if err != nil {
		return err
	}
	var createArguments []string
	for _, slice := range slicesToBuild {
		products := filepath.Join(b.build, "moltenvk-"+slice.sdk)
		err = os.RemoveAll(products)
		if err == nil {
			err = tool.Run(exec.Command("xcodebuild", "build", "-quiet",
				"-project", filepath.Join(moltenVKSourcePath, "MoltenVK/MoltenVK.xcodeproj"),
				"-target", "MoltenVK-iOS-dynamic",
				"-configuration", "Release",
				"-sdk", slice.sdk,
				"ARCHS=arm64",
				"ONLY_ACTIVE_ARCH=NO",
				"IPHONEOS_DEPLOYMENT_TARGET="+iosVersion,
				"SYMROOT="+filepath.Join(products, "Products"),
				"OBJROOT="+filepath.Join(products, "Intermediates"),
				"PRODUCT_BUNDLE_IDENTIFIER=com.khronos.MoltenVK",
				"EXPORTED_SYMBOLS_FILE="+exportsPath,
				"DEAD_CODE_STRIPPING=YES",
				"OTHER_LDFLAGS=-all_load -w -Wl,-dead_strip_dylibs",
				"CODE_SIGNING_ALLOWED=NO",
				"GCC_PREPROCESSOR_DEFINITIONS=$(inherited) MVK_HIDE_VULKAN_SYMBOLS=1 MVK_USE_METAL_PRIVATE_API=0",
			))
		}
		if err != nil {
			return err
		}
		framework := filepath.Join(products, "Products", "Release-"+slice.sdk, moltenVK+".framework")
		err = tool.Run(exec.Command("strip", "-x", filepath.Join(framework, moltenVK)))
		if err != nil {
			return err
		}
		createArguments = append(createArguments, "-framework", framework)
	}
	return createXCFramework(moltenVK, createArguments)
}

func createXCFramework(name string, createArguments []string) error {
	xcframework := filepath.Join(frameworksPath, name+".xcframework")
	archive := filepath.Join(buildPath, name+".xcframework.zip")
	err := os.RemoveAll(xcframework)
	if err == nil {
		err = tool.Run(exec.Command("xcodebuild", append(append([]string{"-create-xcframework"}, createArguments...), "-output", xcframework)...))
	}
	if err == nil {
		err = os.MkdirAll(buildPath, 0o755)
	}
	if err == nil {
		err = os.RemoveAll(archive)
	}
	if err != nil {
		return err
	}
	return tool.Run(exec.Command("ditto", "-c", "-k", "--norsrc", "--noextattr", "--keepParent", xcframework, archive))
}

func (b *builder) pythonEnvironment() error {
	_, err := os.Stat(filepath.Join(b.venv, "bin", "python3"))
	if err == nil {
		return nil
	}
	err = tool.Run(exec.Command("python3", "-m", "venv", b.venv))
	if err != nil {
		return err
	}
	return tool.Run(exec.Command(filepath.Join(b.venv, "bin", "pip"), "install", "-q", "mako", "pyyaml", "packaging", "meson>=1.9.1"))
}

func (b *builder) checkout(repo repository) error {
	directory := filepath.Join(b.source, repo.directory)
	_, err := os.Stat(filepath.Join(directory, ".git"))
	if err != nil {
		err = tool.Run(exec.Command("git", "clone", "--filter=blob:none", "--no-checkout", repo.url, directory))
		if err != nil {
			return err
		}
	}
	if exec.Command("git", "-C", directory, "cat-file", "-e", repo.commit+"^{commit}").Run() != nil {
		err = tool.Run(exec.Command("git", "-C", directory, "fetch", "--filter=blob:none", "origin", repo.commit))
		if err != nil {
			return err
		}
	}
	err = tool.Run(exec.Command("git", "-C", directory, "checkout", "--force", "--detach", repo.commit))
	if err == nil {
		err = tool.Run(exec.Command("git", "-C", directory, "clean", "-fdx", "-q"))
	}
	if err != nil {
		return err
	}
	patchDirectory := path.Join("patches", strings.ToLower(repo.directory))
	entries, err := fs.ReadDir(patches, patchDirectory)
	if errors.Is(err, fs.ErrNotExist) {
		return nil
	} else if err != nil {
		return err
	}
	for _, entry := range entries {
		patch, err := patches.ReadFile(path.Join(patchDirectory, entry.Name()))
		if err != nil {
			return err
		}
		apply := exec.Command("git", "-C", directory, "apply", "--whitespace=nowarn", "-")
		apply.Stdin = bytes.NewReader(patch)
		err = tool.Run(apply)
		if err != nil {
			return E.Cause(err, "apply ", entry.Name())
		}
	}
	return nil
}
