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
            name: "CSwiptermKit",
            url: "https://github.com/swipt-automation/SwiptermKit/releases/download/binary.73fe7a02baf9/CSwiptermKit.xcframework.zip",
            checksum: "6982f48203294f6d7134853bb7f3f72fe4e2f7f0c5a1fc8e0390291d6b7b707c"
        ),
        .target(
            name: "SwiptermKit",
            dependencies: ["CSwiptermKit"],
            resources: [
                .copy("Resources/Image.gz"),
                .copy("Resources/initramfs.cpio.gz"),
            ],
            linkerSettings: [
                .linkedLibrary("archive"),
                .linkedLibrary("lzma"),
                .linkedLibrary("resolv"),
                .linkedLibrary("z"),
            ]
        ),
    ]
)
