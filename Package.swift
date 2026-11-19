// swift-tools-version: 6.2

import PackageDescription

let package = Package(
    name: "SwiptermKit",
    platforms: [.iOS(.v16)],
    products: [
        .library(name: "SwiptermKit", targets: ["SwiptermKit"]),
    ],
    targets: [
        .binaryTarget(
            name: "CSwiptermKit",
            url: "https://github.com/swipt-automation/libswipterm-spm/releases/download/binary.0f00958b0443/CSwiptermKit.xcframework.zip",
            checksum: "e695accba0a0f4b44ed0ddafa988ffd44cf212d63f7b4ff37f84399fad000067"
        ),
        .target(
            name: "SwiptermKit",
            dependencies: ["CSwiptermKit"],
            linkerSettings: [
                .linkedLibrary("archive"),
                .linkedLibrary("resolv"),
                .linkedLibrary("sqlite3"),
            ]
        ),
    ]
)
