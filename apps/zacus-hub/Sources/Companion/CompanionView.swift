import SwiftUI
#if os(iOS)
import UniformTypeIdentifiers
#endif

struct CompanionView: View {
    @EnvironmentObject var session: HubSession
    @StateObject private var recorder = AudioRecorder()

    @State private var hint: String = ""
    @State private var sceneId: String = "intro"
    @State private var level: Int = 1
    @State private var hintLoading = false

    @State private var transcription: String = ""
    @State private var transcribeLoading = false
    @State private var pickerPresented = false
    @State private var elapsed: TimeInterval = 0

    @State private var error: String?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 24) {
                header
                pushToTalkCard
                hintCard
                transcribeCard
                if let error { Text(error).font(.caption).foregroundStyle(.red) }
            }
            .padding()
        }
        .navigationTitle("Companion")
        #if os(iOS)
        .fileImporter(
            isPresented: $pickerPresented,
            allowedContentTypes: [.audio, UTType(filenameExtension: "wav") ?? .data, UTType(filenameExtension: "m4a") ?? .data],
            allowsMultipleSelection: false
        ) { result in
            Task { await handlePicked(result) }
        }
        #endif
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("Companion").font(.largeTitle.bold())
            Text("Push-to-talk, transcription, indices.").foregroundStyle(.secondary)
        }
    }

    // MARK: Push-to-talk

    private var pushToTalkCard: some View {
        GroupBox("Push-to-talk") {
            VStack(spacing: 16) {
                ZStack {
                    Circle()
                        .fill(micFill)
                        .frame(width: 140, height: 140)
                        .shadow(radius: isRecording ? 12 : 4)
                        .scaleEffect(isRecording ? 1.06 : 1)
                        .animation(.easeInOut(duration: 0.25), value: isRecording)
                    Image(systemName: micIcon)
                        .font(.system(size: 56, weight: .bold))
                        .foregroundStyle(.white)
                }
                .gesture(
                    LongPressGesture(minimumDuration: 0.05)
                        .sequenced(before: DragGesture(minimumDistance: 0))
                        .onChanged { _ in if !isRecording { Task { await recorder.start() } } }
                        .onEnded { _ in Task { await finishRecording() } }
                )
                Text(statusText)
                    .font(.subheadline.monospacedDigit())
                    .foregroundStyle(.secondary)
                Text("Maintiens pour parler, relâche pour transcrire.")
                    .font(.caption).foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
                if !transcription.isEmpty {
                    Text(transcription)
                        .font(.body)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(.top, 4)
                }
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 8)
            .onReceive(Timer.publish(every: 0.1, on: .main, in: .common).autoconnect()) { _ in tickElapsed() }
        }
    }

    private var isRecording: Bool {
        if case .recording = recorder.state { return true } else { return false }
    }

    private var micFill: Color {
        switch recorder.state {
        case .recording: return .red
        case .stopping, .requestingPermission: return .orange
        case .denied, .failed: return .gray
        default: return transcribeLoading ? .orange : .accentColor
        }
    }

    private var micIcon: String {
        switch recorder.state {
        case .recording: return "stop.circle.fill"
        case .stopping, .requestingPermission: return "ellipsis.circle"
        case .denied: return "mic.slash"
        case .failed: return "exclamationmark.triangle"
        default: return transcribeLoading ? "waveform" : "mic.fill"
        }
    }

    private var statusText: String {
        switch recorder.state {
        case .idle:                  return transcribeLoading ? "Transcription…" : "Prêt"
        case .requestingPermission:  return "Autorisation micro…"
        case .recording:             return String(format: "● Enregistrement %.1f s", elapsed)
        case .stopping:              return "Finalisation…"
        case .denied:                return "Micro refusé — autorise dans Réglages."
        case .failed(let m):         return "Erreur: \(m)"
        }
    }

    private func tickElapsed() {
        if case .recording(let start) = recorder.state {
            elapsed = Date().timeIntervalSince(start)
        } else if elapsed != 0 {
            elapsed = 0
        }
    }

    private func finishRecording() async {
        guard let url = await recorder.stop() else { return }
        await transcribe(url: url)
    }

    // MARK: Hint

    private var hintCard: some View {
        GroupBox("Demander un indice") {
            VStack(alignment: .leading, spacing: 12) {
                TextField("Scène", text: $sceneId).textFieldStyle(.roundedBorder)
                Stepper("Niveau \(level)", value: $level, in: 1...3)
                Button {
                    Task { await askHint() }
                } label: {
                    Label(hintLoading ? "…" : "Obtenir un indice", systemImage: "lightbulb")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .disabled(hintLoading || sceneId.isEmpty)
                if !hint.isEmpty {
                    Text(hint).font(.body).padding(.top, 4)
                }
            }
        }
    }

    // MARK: Transcribe from file

    private var transcribeCard: some View {
        GroupBox("Ou transcrire un fichier") {
            VStack(alignment: .leading, spacing: 12) {
                Text("Choisis un .wav/.m4a déjà enregistré.")
                    .font(.caption).foregroundStyle(.secondary)
                Button {
                    #if os(iOS)
                    pickerPresented = true
                    #endif
                } label: {
                    Label(transcribeLoading ? "…" : "Choisir un fichier", systemImage: "waveform.badge.magnifyingglass")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .disabled(transcribeLoading)
            }
        }
    }

    private func askHint() async {
        hintLoading = true
        defer { hintLoading = false }
        do {
            let resp = try await session.api.askHint(scene: sceneId, level: level)
            hint = resp.hint ?? "(pas de réponse)"
            error = nil
        } catch let err { self.error = err.localizedDescription }
    }

    #if os(iOS)
    private func handlePicked(_ result: Result<[URL], Error>) async {
        switch result {
        case .failure(let err):
            error = err.localizedDescription
        case .success(let urls):
            guard let url = urls.first else { return }
            await transcribe(url: url)
        }
    }
    #endif

    private func transcribe(url: URL) async {
        transcribeLoading = true
        defer { transcribeLoading = false }
        let didStart = url.startAccessingSecurityScopedResource()
        defer { if didStart { url.stopAccessingSecurityScopedResource() } }
        do {
            let data = try Data(contentsOf: url)
            let mime = url.pathExtension.lowercased() == "m4a" ? "audio/mp4" : "audio/wav"
            let resp = try await session.api.transcribe(audio: data, filename: url.lastPathComponent, mime: mime)
            transcription = resp.text ?? resp.raw ?? "(pas de texte)"
            error = nil
        } catch let err { self.error = err.localizedDescription }
    }
}
