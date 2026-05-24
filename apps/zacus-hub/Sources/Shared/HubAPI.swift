import Foundation

enum HubError: Error, LocalizedError {
    case badURL
    case http(Int, String)
    case decode(Error)
    case transport(Error)

    var errorDescription: String? {
        switch self {
        case .badURL: return "Configuration: URL invalide."
        case .http(let code, let body): return "HTTP \(code): \(body)"
        case .decode(let err): return "Decode: \(err.localizedDescription)"
        case .transport(let err): return "Réseau: \(err.localizedDescription)"
        }
    }
}

actor HubAPI {
    private var config: HubConfig
    private let session: URLSession

    init(config: HubConfig) {
        self.config = config
        let cfg = URLSessionConfiguration.default
        cfg.timeoutIntervalForRequest = 20
        cfg.waitsForConnectivity = true
        self.session = URLSession(configuration: cfg)
    }

    func update(config: HubConfig) { self.config = config }
    func currentConfig() -> HubConfig { config }

    // MARK: - State + health

    func ping() async throws { _ = try await get("/v1/auth/ping") as PingResponse }
    func fetchState() async throws -> GameState { try await get("/v1/state") }
    func fetchHealth() async throws -> [BackendHealth] { try await get("/v1/backends/health") }

    // MARK: - Game master

    func triggerGMHint(scene: String, level: Int) async throws -> HintResponse {
        try await post("/v1/gm/hint", body: ["scene": scene, "level": level])
    }

    // MARK: - Companion

    func askHint(scene: String, level: Int) async throws -> HintResponse {
        try await post("/v1/companion/hint", body: ["scene": scene, "level": level])
    }

    func transcribe(audio: Data, filename: String, mime: String) async throws -> TranscriptionResponse {
        let boundary = "Boundary-\(UUID().uuidString)"
        var req = try makeRequest("/v1/companion/voice/transcribe", method: "POST")
        req.setValue("multipart/form-data; boundary=\(boundary)", forHTTPHeaderField: "Content-Type")
        var body = Data()
        body.appendString("--\(boundary)\r\n")
        body.appendString("Content-Disposition: form-data; name=\"audio\"; filename=\"\(filename)\"\r\n")
        body.appendString("Content-Type: \(mime)\r\n\r\n")
        body.append(audio)
        body.appendString("\r\n--\(boundary)--\r\n")
        req.httpBody = body
        return try await run(req)
    }

    // MARK: - Studio

    func listScenarios() async throws -> [ScenarioMeta] {
        try await get("/v1/studio/scenarios")
    }

    func loadScenario(name: String) async throws -> ScenarioDetail {
        try await get("/v1/studio/scenario/\(name)")
    }

    func saveScenario(name: String, yaml: String) async throws -> ValidationResult {
        var req = try makeRequest("/v1/studio/scenario/\(name)", method: "PUT")
        req.httpBody = try JSONSerialization.data(withJSONObject: ["yaml": yaml])
        return try await run(req)
    }

    func compileScenario(name: String) async throws -> CompileResult {
        var req = try makeRequest("/v1/studio/scenario/\(name)/compile", method: "POST")
        req.httpBody = Data("{}".utf8)
        return try await run(req)
    }

    func listBoards() async throws -> [BoardInfo] {
        try await get("/v1/flash/boards")
    }

    func flashBoard(_ board: String, scenario: String, strategy: String) async throws -> FlashResult {
        try await post("/v1/flash/\(board)", body: ["scenario": scenario, "strategy": strategy])
    }

    func validateScenario(name: String, yaml: String? = nil) async throws -> ValidationResult {
        if let yaml {
            return try await post("/v1/studio/scenario/\(name)/validate", body: ["yaml": yaml])
        }
        var req = try makeRequest("/v1/studio/scenario/\(name)/validate", method: "POST")
        req.httpBody = Data("{}".utf8)
        return try await run(req)
    }

    // MARK: - WebSocket

    func stateWebSocketURL() throws -> URL {
        guard var components = URLComponents(string: config.baseURL) else { throw HubError.badURL }
        let scheme = (components.scheme ?? "http") == "https" ? "wss" : "ws"
        components.scheme = scheme
        components.path = (components.path.isEmpty ? "" : components.path) + "/v1/state/ws"
        components.queryItems = [URLQueryItem(name: "token", value: config.token)]
        guard let url = components.url else { throw HubError.badURL }
        return url
    }

    func openStateSocket() throws -> URLSessionWebSocketTask {
        let url = try stateWebSocketURL()
        let task = session.webSocketTask(with: url)
        task.resume()
        return task
    }

    // MARK: - helpers

    private func makeRequest(_ path: String, method: String) throws -> URLRequest {
        guard var components = URLComponents(string: config.baseURL) else { throw HubError.badURL }
        components.path = (components.path.isEmpty ? "" : components.path) + path
        guard let url = components.url else { throw HubError.badURL }
        var req = URLRequest(url: url)
        req.httpMethod = method
        req.setValue("Bearer \(config.token)", forHTTPHeaderField: "Authorization")
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        return req
    }

    private func get<T: Decodable>(_ path: String) async throws -> T {
        try await run(makeRequest(path, method: "GET"))
    }

    private func post<T: Decodable>(_ path: String, body: [String: Any]) async throws -> T {
        var req = try makeRequest(path, method: "POST")
        req.httpBody = try JSONSerialization.data(withJSONObject: body)
        return try await run(req)
    }

    private func run<T: Decodable>(_ req: URLRequest) async throws -> T {
        let (data, resp): (Data, URLResponse)
        do { (data, resp) = try await session.data(for: req) }
        catch { throw HubError.transport(error) }
        guard let http = resp as? HTTPURLResponse, 200..<300 ~= http.statusCode else {
            let code = (resp as? HTTPURLResponse)?.statusCode ?? -1
            let body = String(data: data, encoding: .utf8) ?? ""
            throw HubError.http(code, body)
        }
        do { return try JSONDecoder().decode(T.self, from: data) }
        catch { throw HubError.decode(error) }
    }
}

// MARK: - DTOs

struct PingResponse: Decodable { let status: String }

struct BackendHealth: Codable, Identifiable, Hashable, Equatable {
    let name: String
    let url: String?
    let online: Bool
    let latency_ms: Double?
    let detail: String?
    var id: String { name }
}

struct ScenarioMeta: Decodable, Identifiable, Hashable {
    let name: String
    let path: String
    let size: Int
    let modified: Double
    var id: String { name }
}

struct ScenarioDetail: Decodable {
    let name: String
    let yaml: String
    let modified: Double?
}

struct ValidationResult: Decodable {
    let ok: Bool
    let errors: [String]
    let warnings: [String]
    let top_level_keys: [String]
}

struct CompileResult: Decodable {
    let ok: Bool
    let steps_count: Int
    let entry_step_id: String?
    let errors: [String]
    let warnings: [String]
}

struct BoardInfo: Decodable, Identifiable, Hashable {
    let name: String
    let label: String
    let type: String
    let ip: String?
    let mdns: String?
    let hot_endpoint: String?
    let cold_data_dir: String?
    let espnow_relay_peers: [String]
    var id: String { name }
}

struct FlashStep: Decodable, Hashable {
    let label: String
    let status: String
    let detail: String
}

struct FlashResult: Decodable {
    let board: String
    let strategy_used: String
    let ok: Bool
    let steps: [FlashStep]
    let ir_path: String?
    let cold_command: String?
    let relayed_to: [String]
}

struct HintResponse: Decodable {
    let hint: String?
    let level: Int?
}

struct TranscriptionResponse: Decodable {
    let text: String?
    let language: String?
    let raw: String?
}

private extension Data {
    mutating func appendString(_ s: String) { append(Data(s.utf8)) }
}
