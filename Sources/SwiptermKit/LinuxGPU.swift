import CSwiptermKit

extension LinuxRuntime {
    /// A Vulkan driver the guest's Venus driver runs on, loaded from the app's
    /// `Frameworks/<Name>.framework`.
    public enum GPUBackend: Hashable, Sendable, CaseIterable {
        case moltenVK
        /// Requires iOS 26 or macOS 26 on a GPU that supports Metal 4.
        case kosmicKrisp

        var cValue: swipterm_gpu_backend {
            switch self {
            case .moltenVK:
                SWIPTERM_GPU_BACKEND_MOLTENVK
            case .kosmicKrisp:
                SWIPTERM_GPU_BACKEND_KOSMICKRISP
            }
        }
    }

    public enum GPUConfiguration: Hashable, Sendable {
        /// KosmicKrisp when it can be loaded, otherwise MoltenVK.
        case automatic
        case specific(GPUBackend)

        var cValue: swipterm_gpu_options {
            let backend: swipterm_gpu_backend = switch self {
            case .automatic:
                SWIPTERM_GPU_BACKEND_AUTOMATIC
            case .specific(let backend):
                backend.cValue
            }
            return swipterm_gpu_options(enabled: true, backend: backend)
        }
    }

    /// Backends embedded in the app that this device can run.
    public static var availableGPUBackends: [GPUBackend] {
        let mask = swipterm_gpu_available_backends()
        return GPUBackend.allCases.filter { mask & (1 << $0.cValue.rawValue) != 0 }
    }

    /// Whether the runtime booted with a guest GPU.
    public var isGPUActive: Bool {
        (try? withActiveHandle { swipterm_gpu_is_active($0) }) ?? false
    }

    /// Holds new GPU work and waits up to two seconds for the work already submitted to finish.
    /// Call it synchronously before the app moves to the background, while it can still use the GPU.
    /// Returns `false` when the work did not finish in time.
    @discardableResult
    public static func suspendGPU() -> Bool {
        swipterm_gpu_suspend()
    }

    /// Releases GPU work held by `suspendGPU()`.
    public static func resumeGPU() {
        swipterm_gpu_resume()
    }
}
