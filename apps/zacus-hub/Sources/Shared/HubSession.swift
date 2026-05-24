import Foundation
import SwiftUI

@MainActor
final class HubSession: ObservableObject {
    @Published var config: HubConfig
    @Published var authStatus: AuthStatus = .unknown
    @Published var lastError: String?
    @Published var state: GameState = GameState()
    @Published var streamActive: Bool = false

    let api: HubAPI
    private var streamTask: Task<Void, Never>?

    init() {
        let cfg = HubConfig.load()
        self.config = cfg
        self.api = HubAPI(config: cfg)
    }

    func bootstrap() async {
        await refreshAuth()
        startStream()
    }

    func updateConfig(_ new: HubConfig) {
        config = new
        new.save()
        Task {
            await api.update(config: new)
            await refreshAuth()
            restartStream()
        }
    }

    func refreshAuth() async {
        do {
            try await api.ping()
            authStatus = .ok
            lastError = nil
        } catch {
            authStatus = .failed
            lastError = (error as? LocalizedError)?.errorDescription ?? String(describing: error)
        }
    }

    func restartStream() {
        streamTask?.cancel()
        streamActive = false
        startStream()
    }

    private func startStream() {
        guard !config.token.isEmpty else { return }
        streamTask = Task { [weak self] in
            guard let self else { return }
            while !Task.isCancelled {
                do {
                    let socket = try await self.api.openStateSocket()
                    await MainActor.run { self.streamActive = true }
                    try await self.consume(socket: socket)
                } catch {
                    await MainActor.run {
                        self.streamActive = false
                        self.lastError = (error as? LocalizedError)?.errorDescription ?? String(describing: error)
                    }
                }
                try? await Task.sleep(nanoseconds: 3_000_000_000)
            }
            await MainActor.run { self.streamActive = false }
        }
    }

    private func consume(socket: URLSessionWebSocketTask) async throws {
        while !Task.isCancelled {
            let message = try await socket.receive()
            switch message {
            case .string(let s):
                if let data = s.data(using: .utf8), let decoded = try? JSONDecoder().decode(GameState.self, from: data) {
                    await MainActor.run { self.state = decoded }
                }
            case .data(let d):
                if let decoded = try? JSONDecoder().decode(GameState.self, from: d) {
                    await MainActor.run { self.state = decoded }
                }
            @unknown default:
                continue
            }
        }
    }
}

enum AuthStatus { case unknown, ok, failed }

struct HubConfig: Codable, Equatable {
    var baseURL: String
    var token: String

    // Defaults are deliberately credential-free — never commit a token here.
    // Users paste their gateway token via Settings on first launch; it lands
    // in the system Keychain via `KeychainStore.setToken`.
    static let defaults = HubConfig(
        baseURL: "http://electron-server:8400",
        token: ""
    )

    static func load() -> HubConfig {
        let defs = UserDefaults.standard
        let storedToken = KeychainStore.token() ?? ""
        return HubConfig(
            baseURL: defs.string(forKey: "hub.baseURL") ?? defaults.baseURL,
            token: storedToken
        )
    }

    func save() {
        UserDefaults.standard.set(baseURL, forKey: "hub.baseURL")
        KeychainStore.setToken(token)
    }
}

struct GameState: Codable, Equatable {
    var scene_id: String?
    var scene_index: Int = 0
    var last_hint: String?
    var voice_session: String?
    var backends: [BackendHealth] = []
}

