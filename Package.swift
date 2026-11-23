// swift-tools-version: 6.2

import PackageDescription

let package = Package(
    name: "SwiptermKit",
    platforms: [.iOS(.v17), .macOS(.v14)],
    products: [
        .library(name: "SwiptermKit", targets: ["SwiptermKit"]),
    ],
    targets: [
        .binaryTarget(
            name: "LibSwipterm",
            url: "https://github.com/swipt-automation/SwiptermKit/releases/download/binary.95adc9cd6d95/LibSwipterm.xcframework.zip",
            checksum: "645dfcfd8e257f6ae3d19751b260008de568ee70ad0645a0e813eddda1b586a5"
        ),
        .target(
            name: "CSwiptermKit",
            dependencies: ["LibSwipterm"],
            path: "VMM/include/CSwiptermKit",
            publicHeadersPath: ".",
            linkerSettings: [
                .linkedLibrary("archive"),
                .linkedLibrary("lzma"),
                .linkedLibrary("resolv"),
                .linkedLibrary("z"),
            ]
        ),
        .target(
            name: "SwiptermKit",
            dependencies: ["CSwiptermKit", "LibSwipterm"],
            resources: [
                .copy("Resources/initramfs.cpio.gz"),
            ]
        ),
    ]
)
