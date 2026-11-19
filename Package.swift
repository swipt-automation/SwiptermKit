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
            path: "Frameworks/CSwiptermKit.xcframework"
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
