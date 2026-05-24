import SwiftUI
import WebKit
#if os(iOS)
import UniformTypeIdentifiers
#endif

/// Blockly-in-WKWebView Scratch-like editor. Loads `Resources/blockly/editor.html`
/// (which pulls Blockly + JSZip from CDN) and bridges save / sb3 export to the gateway.
struct ScratchEditorView: View {
    @EnvironmentObject var session: HubSession
    let scenarioName: String

    @State private var bridge = WebBridge()
    @State private var status: String = "chargement…"
    @State private var lastError: String?
    @State private var isReady = false
    @State private var saving = false
    @State private var showFlasher = false

    var body: some View {
        VStack(spacing: 0) {
            toolbar
            Divider()
            BlocklyWebView(bridge: bridge,
                           onMessage: handle,
                           onReady: { isReady = true; Task { await loadFromGateway() } })
        }
        .navigationTitle("Scratch · \(scenarioName)")
    }

    private var toolbar: some View {
        HStack(spacing: 8) {
            Text("Scénario: \(scenarioName)").font(.caption.monospaced()).foregroundStyle(.secondary)
            Spacer()
            Text(status).font(.caption).foregroundStyle(.secondary).lineLimit(1)
            if let lastError {
                Text(lastError).font(.caption).foregroundStyle(.red).lineLimit(1)
            }
            Button {
                Task { await loadFromGateway() }
            } label: { Label("Recharger", systemImage: "arrow.clockwise") }
                .disabled(!isReady)
            Button {
                showFlasher = true
            } label: { Label("Flasher", systemImage: "bolt.fill") }
                .buttonStyle(.borderedProminent)
                .disabled(!isReady)
        }
        .padding(8)
        .sheet(isPresented: $showFlasher) {
            FlasherSheet(scenarioName: scenarioName).environmentObject(session)
        }
    }

    // MARK: bridge handling

    private func handle(_ payload: [String: Any]) {
        guard let op = payload["op"] as? String else { return }
        switch op {
        case "ready":
            status = "prêt"
        case "save":
            if let yaml = payload["yaml"] as? String {
                Task { await pushToGateway(yaml: yaml) }
            }
        case "exportSB3":
            if let b64 = payload["base64"] as? String, let data = Data(base64Encoded: b64) {
                Task { await saveSB3Locally(data: data) }
            }
        case "error":
            lastError = payload["message"] as? String
        default:
            break
        }
    }

    private func loadFromGateway() async {
        do {
            let detail = try await session.api.loadScenario(name: scenarioName)
            status = "chargé"
            lastError = nil
            await MainActor.run {
                let safeYaml = jsString(detail.yaml)
                bridge.run("window.zacus && window.zacus.loadYAML(\(safeYaml))")
            }
        } catch let err {
            lastError = err.localizedDescription
        }
    }

    private func pushToGateway(yaml: String) async {
        saving = true; defer { saving = false }
        do {
            _ = try await session.api.saveScenario(name: scenarioName, yaml: yaml)
            status = "enregistré · \(timestamp())"
            lastError = nil
        } catch let err {
            lastError = err.localizedDescription
        }
    }

    private func saveSB3Locally(data: Data) async {
        let fm = FileManager.default
        let baseURL: URL
        #if os(macOS)
        baseURL = fm.urls(for: .downloadsDirectory, in: .userDomainMask).first ?? fm.temporaryDirectory
        #else
        baseURL = fm.urls(for: .documentDirectory, in: .userDomainMask).first ?? fm.temporaryDirectory
        #endif
        let file = baseURL.appendingPathComponent("\(scenarioName).sb3")
        do {
            try data.write(to: file)
            status = "exporté vers \(file.lastPathComponent)"
            #if os(macOS)
            NSWorkspace.shared.activateFileViewerSelecting([file])
            #endif
        } catch {
            lastError = "écriture .sb3: \(error.localizedDescription)"
        }
    }

    private func jsString(_ s: String) -> String {
        // wrap as a JSON string so embedded quotes/newlines survive JS eval
        guard let data = try? JSONSerialization.data(withJSONObject: [s]),
              let text = String(data: data, encoding: .utf8) else {
            return "\"\""
        }
        // strip [" ... "] → keep the inner JSON string literal
        let trimmed = text.dropFirst().dropLast()
        return String(trimmed)
    }

    private func timestamp() -> String {
        let f = DateFormatter(); f.dateFormat = "HH:mm:ss"; return f.string(from: Date())
    }
}

// MARK: - Bridge container

final class WebBridge {
    weak var webView: WKWebView?
    func run(_ js: String) {
        webView?.evaluateJavaScript(js, completionHandler: nil)
    }
}

// MARK: - WKWebView SwiftUI wrapper

#if os(macOS)
struct BlocklyWebView: NSViewRepresentable {
    let bridge: WebBridge
    let onMessage: ([String: Any]) -> Void
    let onReady: () -> Void

    func makeNSView(context: Context) -> WKWebView {
        let webView = makeWebView(coordinator: context.coordinator)
        bridge.webView = webView
        return webView
    }
    func updateNSView(_ nsView: WKWebView, context: Context) {}
    func makeCoordinator() -> Coordinator { Coordinator(onMessage: onMessage, onReady: onReady) }
    final class Coordinator: NSObject, WKScriptMessageHandler {
        let onMessage: ([String: Any]) -> Void
        let onReady: () -> Void
        init(onMessage: @escaping ([String: Any]) -> Void, onReady: @escaping () -> Void) {
            self.onMessage = onMessage; self.onReady = onReady
        }
        func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
            guard let body = message.body as? [String: Any] else { return }
            DispatchQueue.main.async {
                self.onMessage(body)
                if (body["op"] as? String) == "ready" { self.onReady() }
            }
        }
    }
}
#else
struct BlocklyWebView: UIViewRepresentable {
    let bridge: WebBridge
    let onMessage: ([String: Any]) -> Void
    let onReady: () -> Void

    func makeUIView(context: Context) -> WKWebView {
        let webView = makeWebView(coordinator: context.coordinator)
        bridge.webView = webView
        return webView
    }
    func updateUIView(_ uiView: WKWebView, context: Context) {}
    func makeCoordinator() -> Coordinator { Coordinator(onMessage: onMessage, onReady: onReady) }
    final class Coordinator: NSObject, WKScriptMessageHandler {
        let onMessage: ([String: Any]) -> Void
        let onReady: () -> Void
        init(onMessage: @escaping ([String: Any]) -> Void, onReady: @escaping () -> Void) {
            self.onMessage = onMessage; self.onReady = onReady
        }
        func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
            guard let body = message.body as? [String: Any] else { return }
            DispatchQueue.main.async {
                self.onMessage(body)
                if (body["op"] as? String) == "ready" { self.onReady() }
            }
        }
    }
}
#endif

private func makeWebView(coordinator: NSObject & WKScriptMessageHandler) -> WKWebView {
    let cfg = WKWebViewConfiguration()
    let ucc = WKUserContentController()
    ucc.add(coordinator, name: "zacus")
    cfg.userContentController = ucc
    let pref = WKPreferences()
    pref.javaScriptCanOpenWindowsAutomatically = false
    cfg.preferences = pref
    let webView = WKWebView(frame: .zero, configuration: cfg)
    if let url = Bundle.main.url(forResource: "editor", withExtension: "html", subdirectory: "blockly") {
        webView.loadFileURL(url, allowingReadAccessTo: url.deletingLastPathComponent())
    } else if let url = Bundle.main.url(forResource: "editor", withExtension: "html") {
        webView.loadFileURL(url, allowingReadAccessTo: url.deletingLastPathComponent())
    }
    #if os(macOS)
    webView.setValue(false, forKey: "drawsBackground")
    #endif
    return webView
}
