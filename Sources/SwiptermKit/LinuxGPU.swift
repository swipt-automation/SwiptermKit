import CSwiptermKit

public extension LinuxRuntime {
    enum GPUBackend: Hashable, Sendable, CaseIterable {
        case moltenVK
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

    enum GPUConfiguration: Hashable, Sendable {
        case automatic
        case specific(GPUBackend)

        var cValue: swipterm_gpu_options {
            let backend: swipterm_gpu_backend = switch self {
            case .automatic:
                SWIPTERM_GPU_BACKEND_AUTOMATIC
            case let .specific(backend):
                backend.cValue
            }
            return swipterm_gpu_options(enabled: true, backend: backend)
        }
    }

    static var availableGPUBackends: [GPUBackend] {
        let mask = swipterm_gpu_available_backends()
        return GPUBackend.allCases.filter { mask & (1 << $0.cValue.rawValue) != 0 }
    }

    static func suspendGPU() {
        swipterm_gpu_suspend()
    }

    static func resumeGPU() {
        swipterm_gpu_resume()
    }
}
