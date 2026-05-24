import SwiftUI

struct FlasherSheet: View {
    @EnvironmentObject var session: HubSession
    @Environment(\.dismiss) private var dismiss
    let scenarioName: String

    @State private var boards: [BoardInfo] = []
    @State private var selected: Set<String> = []
    @State private var strategy: String = "auto"
    @State private var results: [String: FlashResult] = [:]
    @State private var running: Set<String> = []
    @State private var error: String?

    var body: some View {
        NavigationStack {
            Form {
                Section("Scénario") {
                    Text(scenarioName).font(.body.monospaced())
                    Picker("Stratégie", selection: $strategy) {
                        Text("Auto (hot si IP, sinon cold)").tag("auto")
                        Text("Hot — POST IR via WiFi").tag("hot")
                        Text("Cold — rebuild + flash série").tag("cold")
                    }
                    .pickerStyle(.menu)
                }
                Section("Boards cibles") {
                    if boards.isEmpty {
                        Text("Aucun board déclaré — vérifie `tools/zacus-gateway/boards.yaml` sur electron-server.")
                            .font(.caption).foregroundStyle(.secondary)
                    }
                    ForEach(boards) { board in
                        boardRow(board)
                    }
                }
                if let error { Section { Text(error).foregroundStyle(.red).font(.caption) } }
                if !results.isEmpty {
                    Section("Résultats") {
                        ForEach(Array(results.keys).sorted(), id: \.self) { name in
                            if let r = results[name] { resultBlock(r) }
                        }
                    }
                }
            }
            .navigationTitle("Flasher")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Fermer") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Flasher") { Task { await flashAll() } }
                        .disabled(selected.isEmpty || !running.isEmpty)
                }
            }
            .task { await loadBoards() }
        }
        #if os(macOS)
        .frame(minWidth: 640, minHeight: 520)
        #endif
    }

    private func boardRow(_ board: BoardInfo) -> some View {
        HStack(alignment: .top) {
            Toggle(isOn: Binding(
                get: { selected.contains(board.name) },
                set: { on in if on { selected.insert(board.name) } else { selected.remove(board.name) } }
            )) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(board.label).font(.body)
                    HStack(spacing: 8) {
                        Text(board.type).font(.caption.monospaced()).foregroundStyle(.secondary)
                        if let ip = board.ip { Text(ip).font(.caption2.monospaced()).foregroundStyle(.secondary) }
                        else if !board.espnow_relay_peers.isEmpty {
                            Text("via ESP-NOW relay").font(.caption2).foregroundStyle(.orange)
                        } else {
                            Text("cold-flash uniquement").font(.caption2).foregroundStyle(.secondary)
                        }
                    }
                    if !board.espnow_relay_peers.isEmpty {
                        Text("Pousse en mesh: \(board.espnow_relay_peers.joined(separator: ", "))")
                            .font(.caption2).foregroundStyle(.blue)
                    }
                }
            }
            Spacer()
            if running.contains(board.name) {
                ProgressView().scaleEffect(0.7)
            }
        }
    }

    private func resultBlock(_ r: FlashResult) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Image(systemName: r.ok ? "checkmark.circle.fill" : "exclamationmark.octagon.fill")
                    .foregroundStyle(r.ok ? .green : .red)
                Text("\(r.board) · \(r.strategy_used)").font(.subheadline.bold())
            }
            ForEach(Array(r.steps.enumerated()), id: \.offset) { _, s in
                HStack(alignment: .top) {
                    Text(statusGlyph(s.status)).foregroundStyle(statusColor(s.status))
                    Text(s.label).font(.caption.bold())
                    Text(s.detail).font(.caption).foregroundStyle(.secondary)
                }
            }
            if let cmd = r.cold_command, !cmd.isEmpty {
                Text("Commande locale:").font(.caption.bold()).padding(.top, 2)
                Text(cmd)
                    .font(.caption.monospaced())
                    .textSelection(.enabled)
                    .padding(6)
                    .background(Color.black.opacity(0.08))
                    .cornerRadius(4)
            }
            if !r.relayed_to.isEmpty {
                Text("Mesh: relayé vers \(r.relayed_to.joined(separator: ", "))")
                    .font(.caption).foregroundStyle(.blue)
            }
        }
        .padding(.vertical, 4)
    }

    private func statusGlyph(_ s: String) -> String {
        switch s { case "ok": "✓"; case "warn": "⚠"; case "skip": "◦"; default: "✗" }
    }
    private func statusColor(_ s: String) -> Color {
        switch s { case "ok": .green; case "warn": .orange; case "skip": .secondary; default: .red }
    }

    private func loadBoards() async {
        do { boards = try await session.api.listBoards(); error = nil }
        catch let err { error = err.localizedDescription }
    }

    private func flashAll() async {
        for name in selected {
            running.insert(name)
            do {
                let r = try await session.api.flashBoard(name, scenario: scenarioName, strategy: strategy)
                results[name] = r
            } catch let err {
                results[name] = FlashResult(board: name, strategy_used: strategy, ok: false,
                                            steps: [FlashStep(label: "request", status: "error", detail: err.localizedDescription)],
                                            ir_path: nil, cold_command: nil, relayed_to: [])
            }
            running.remove(name)
        }
    }
}
