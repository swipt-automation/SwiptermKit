// swift-tools-version: 6.2

import PackageDescription

let package = Package(
    name: "SwiptermKit",
    platforms: [.iOS(.v16)],
    products: [
        .library(name: "SwiptermKit", targets: ["SwiptermKit"]),
        .library(name: "SwiptermAlpineRootfs", targets: ["SwiptermAlpineRootfs"]),
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
        .target(
            name: "SwiptermKit",
            dependencies: ["CSwiptermKit"],
            linkerSettings: [
                .linkedLibrary("archive"),
                .linkedLibrary("resolv"),
                .linkedLibrary("sqlite3"),
            ]
        ),
        .target(
            name: "SwiptermAlpineRootfs",
            dependencies: ["SwiptermKit", "CSwiptermAlpineRootfs"]
        ),
        .testTarget(
            name: "SwiptermKitTests",
            dependencies: ["SwiptermKit", "SwiptermAlpineRootfs"]
        ),
        .testTarget(
            name: "SwiptermKitProcessModeTests",
            dependencies: ["SwiptermKit", "SwiptermAlpineRootfs"]
        ),
    ]
)
