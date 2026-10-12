// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "MotorCyclesPatcher",
    platforms: [.macOS(.v13)],
    products: [
        .executable(name: "MotorCycles", targets: ["MotorCycles"]),
        .executable(name: "mcpatch", targets: ["mcpatch"]),
    ],
    targets: [
        .target(name: "MotorCyclesKit"),
        .executableTarget(name: "mcpatch", dependencies: ["MotorCyclesKit"]),
        .executableTarget(name: "MotorCycles", dependencies: ["MotorCyclesKit"]),
    ]
)
