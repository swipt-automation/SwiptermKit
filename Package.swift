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
            url: "https://github.com/swipt-automation/SwiptermKit/releases/download/binary.d7ac99c7c0b5/LibSwipterm.xcframework.zip",
            checksum: "8ebb2b51ea98f4695d91ff18ef971e24f38fe0b5cf403ce5e5aa023c85929521"
        ),
        .target(
            name: "CSwiptermKit",
            dependencies: ["LibSwipterm"],
            path: "VMM/include/CSwiptermKit",
            publicHeadersPath: ".",
            linkerSettings: [
                .linkedLibrary("archive"),
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
