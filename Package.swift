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
            url: "https://github.com/swipt-automation/SwiptermKit/releases/download/binary.1da0d40ce211/LibSwipterm.xcframework.zip",
            checksum: "bcb44a802489e1c9bbc7a9ab361370c01f88cf824b87f245cd036f9d7e3cf458"
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
                .copy("Resources/Image.gz"),
                .copy("Resources/initramfs.cpio.gz"),
            ]
        ),
    ]
)
