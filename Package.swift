// swift-tools-version: 6.2

import PackageDescription

let cpuStats: [CSetting] = Context.environment["SWIPTERM_CPU_STATS"] == "1" ? [.define("CPU_STATS")] : []

let package = Package(
    name: "SwiptermKit",
    platforms: [.iOS(.v17), .macOS(.v14)],
    products: [
        .library(name: "SwiptermKit", targets: ["SwiptermKit"]),
    ],
    targets: [
        .target(
            name: "CSwiptermKit",
            path: "VMM",
            exclude: [
                "cpu/generated",
                "cpu/cpu_fast_decode.inc",
                "cpu/cpu_fast_ops.inc",
                "deps/zstd/LICENSE",
            ],
            sources: [
                "board",
                "cpu",
                "host",
                "mmu",
                "virtio",
                "deps/libslirp/src",
                "deps/zstd/lib/common",
                "deps/zstd/lib/decompress",
            ],
            publicHeadersPath: "include/CSwiptermKit",
            cSettings: [
                .headerSearchPath("deps/libslirp/glib_compat"),
                .headerSearchPath("deps/zstd/lib"),
                .headerSearchPath("deps/zstd/lib/common"),
                .define("ZSTD_DISABLE_ASM"),
                .define("ZSTD_LEGACY_SUPPORT", to: "0"),
                .define("XXH_NAMESPACE", to: "swipterm_zstd_"),
            ] + cpuStats,
            linkerSettings: [
                .linkedLibrary("archive"),
                .linkedLibrary("lzma"),
                .linkedLibrary("resolv"),
                .linkedLibrary("z"),
            ]
        ),
        .target(
            name: "SwiptermKit",
            dependencies: ["CSwiptermKit"],
            resources: [
                .copy("Resources/Image.gz"),
                .copy("Resources/initramfs.cpio.gz"),
            ]
        ),
    ],
    cLanguageStandard: .gnu2x
)
