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
            path: "Frameworks/CSwiptermKit.xcframework"
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
