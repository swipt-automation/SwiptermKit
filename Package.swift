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
            url: "https://github.com/swipt-automation/SwiptermKit/releases/download/binary.8fe2623e23e7/LibSwipterm.xcframework.zip",
            checksum: "17e9240a08b007a1196aee0cd861e81927a77d9aedf5922a780867bb21bba886"
        ),
        .binaryTarget(
            name: "KosmicKrisp",
            url: "https://github.com/swipt-automation/SwiptermKit/releases/download/vulkan.29d690b87393/KosmicKrisp.xcframework.zip",
            checksum: "83d43c5bc5a2bf759cb28f90e75ac2034753db4186c89adc529460466e9d3a4b"
        ),
        .binaryTarget(
            name: "MoltenVK",
            url: "https://github.com/swipt-automation/SwiptermKit/releases/download/vulkan.29d690b87393/MoltenVK.xcframework.zip",
            checksum: "208d7337bc7186831b2f88309c33cf43ae3a026c59eb1296760a9a84dc109043"
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
