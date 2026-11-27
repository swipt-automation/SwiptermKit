import Foundation

final class RuntimeReadiness: @unchecked Sendable {
    private let queue = DispatchQueue(label: "SwiptermKit.RuntimeReadiness")
    private var outcome: Result<Void, any Error>?
    private var requests: [IORequest<Void>] = []

    func resolve(_ outcome: Result<Void, any Error>) {
        queue.async {
            self.outcome = outcome
            for request in self.requests {
                request.complete(outcome)
            }
            self.requests.removeAll()
        }
    }

    func wait() async throws {
        let request = IORequest<Void>()
        try await request.submit(on: queue) {
            if let outcome = self.outcome {
                request.complete(outcome)
            } else {
                self.requests.append(request)
            }
        } withdraw: {
            self.requests.removeAll { $0 === request }
        }
    }
}
