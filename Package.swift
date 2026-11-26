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
            path: "Frameworks/LibSwipterm.xcframework"
        ),
        .binaryTarget(
            name: "KosmicKrisp",
            path: "Frameworks/KosmicKrisp.xcframework"
        ),
        .binaryTarget(
            name: "MoltenVK",
            path: "Frameworks/MoltenVK.xcframework"
        ),
        .target(
            name: "CSwiptermKit",
            dependencies: ["LibSwipterm"],
            path: "VMM/include/CSwiptermKit",
            publicHeadersPath: ".",
            linkerSettings: [
                .linkedLibrary("archive"),
                .linkedLibrary("z"),
                .linkedFramework("Foundation"),
                .linkedFramework("Metal"),
            ]
        ),
        .target(
            name: "SwiptermKit",
            dependencies: [
                "CSwiptermKit",
                "LibSwipterm",
                .target(name: "KosmicKrisp", condition: .when(platforms: [.iOS])),
                .target(name: "MoltenVK", condition: .when(platforms: [.iOS])),
            ],
            resources: [
                .copy("Resources/initramfs.cpio.gz"),
            ]
        ),
    ]
)
