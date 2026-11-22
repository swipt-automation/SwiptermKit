// swift-tools-version: 6.2

import PackageDescription

let package = Package(
    name: "SwiptermKit",
    platforms: [.iOS(.v17), .macOS(.v14)],
    products: [
        .library(name: "SwiptermKit", targets: ["SwiptermKit"]),
        .library(name: "SwiptermAlpineRootfs", targets: ["SwiptermAlpineRootfs"]),
        .library(name: "SwiptermDebianRootfs", targets: ["SwiptermDebianRootfs"]),
    ],
    targets: [
        .binaryTarget(
            name: "CSwiptermKit",
            path: "Frameworks/CSwiptermKit.xcframework"
        ),
        .binaryTarget(
            name: "CSwiptermAlpineRootfs",
            path: "Frameworks/CSwiptermAlpineRootfs.xcframework"
        ),
        .binaryTarget(
            name: "CSwiptermDebianRootfs",
            path: "Frameworks/CSwiptermDebianRootfs.xcframework"
        ),
        .target(
            name: "SwiptermKit",
            dependencies: ["CSwiptermKit"],
            linkerSettings: [
                .linkedLibrary("archive"),
                .linkedLibrary("lzma"),
                .linkedLibrary("resolv"),
                .linkedLibrary("z"),
            ]
        ),
        .target(
            name: "SwiptermAlpineRootfs",
            dependencies: ["SwiptermKit", "CSwiptermAlpineRootfs"]
        ),
        .target(
            name: "SwiptermDebianRootfs",
            dependencies: ["SwiptermKit", "CSwiptermDebianRootfs"]
        ),
    ]
)
