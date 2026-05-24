import SwiftUI

struct GameMasterView: View {
    @EnvironmentObject var session: HubSession
    @State private var hintScene: String = "intro"
    @State private var hintLevel: Int = 1
    @State private var actionLog: [String] = []
    @State private var sending = false

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 24) {
                header
                backendsSection
                sceneSection
                actionsSection
                if !actionLog.isEmpty { logSection }
                if let error = session.lastError { Text(error).font(.callout).foregroundStyle(.red) }
            }
            .padding()
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .navigationTitle("Game master")
    }

    private var header: some View {
        HStack(alignment: .firstTextBaseline) {
            VStack(alignment: .leading, spacing: 4) {
                Text("Live").font(.caption.bold()).foregroundStyle(.secondary)
                Text(session.streamActive ? "Connecté au gateway" : "Hors ligne")
                    .font(.title3.bold())
                    .foregroundStyle(session.streamActive ? .green : .orange)
            }
            Spacer()
            Image(systemName: session.streamActive ? "antenna.radiowaves.left.and.right" : "antenna.radiowaves.left.and.right.slash")
                .font(.title)
                .foregroundStyle(session.streamActive ? .green : .orange)
        }
    }

    private var backendsSection: some View {
        GroupBox("Backends") {
            VStack(alignment: .leading, spacing: 8) {
                if session.state.backends.isEmpty {
                    Text("En attente de la première trame WS…").font(.caption).foregroundStyle(.secondary)
                }
                ForEach(session.state.backends) { backend in
                    HStack(spacing: 10) {
                        Circle().fill(backend.online ? Color.green : Color.red).frame(width: 10, height: 10)
                        Text(backend.name).font(.body.monospaced())
                        Spacer()
                        if let lat = backend.latency_ms {
                            Text("\(Int(lat)) ms").font(.caption).foregroundStyle(.secondary)
                        }
                        if let detail = backend.detail {
                            Text(detail).font(.caption2).foregroundStyle(.secondary).lineLimit(1)
                        }
                    }
                }
            }
        }
    }

    private var sceneSection: some View {
        GroupBox("Partie") {
            Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 8) {
                GridRow {
                    Text("Scène").font(.caption).foregroundStyle(.secondary)
                    Text(session.state.scene_id ?? "—").font(.body.monospaced())
                }
                GridRow {
                    Text("Index").font(.caption).foregroundStyle(.secondary)
                    Text("\(session.state.scene_index)").font(.body.monospaced())
                }
                GridRow {
                    Text("Voice session").font(.caption).foregroundStyle(.secondary)
                    Text(session.state.voice_session ?? "—").font(.body.monospaced())
                }
                GridRow {
                    Text("Dernier indice").font(.caption).foregroundStyle(.secondary)
                    Text(session.state.last_hint ?? "—").lineLimit(3)
                }
            }
        }
    }

    private var actionsSection: some View {
        GroupBox("Déclencher un indice") {
            VStack(alignment: .leading, spacing: 12) {
                TextField("Scène", text: $hintScene).textFieldStyle(.roundedBorder)
                Stepper("Niveau \(hintLevel)", value: $hintLevel, in: 1...3)
                Button {
                    Task { await triggerHint() }
                } label: {
                    Label(sending ? "Envoi…" : "Envoyer au moteur d'indices", systemImage: "lightbulb.fill")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .disabled(sending || hintScene.isEmpty)
            }
        }
    }

    private var logSection: some View {
        GroupBox("Journal") {
            VStack(alignment: .leading, spacing: 4) {
                ForEach(Array(actionLog.enumerated()), id: \.offset) { _, entry in
                    Text(entry).font(.caption.monospaced()).foregroundStyle(.secondary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private func triggerHint() async {
        sending = true
        defer { sending = false }
        do {
            let resp = try await session.api.triggerGMHint(scene: hintScene, level: hintLevel)
            actionLog.insert("[\(timestamp())] hint L\(hintLevel) → \(resp.hint ?? "(vide)")", at: 0)
        } catch {
            actionLog.insert("[\(timestamp())] erreur: \(error.localizedDescription)", at: 0)
        }
    }

    private func timestamp() -> String {
        let f = DateFormatter(); f.dateFormat = "HH:mm:ss"; return f.string(from: Date())
    }
}
