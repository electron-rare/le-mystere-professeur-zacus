import SwiftUI

enum StudioMode: String, CaseIterable, Identifiable {
    case scratch, yaml
    var id: String { rawValue }
    var label: String {
        switch self { case .yaml: "YAML"; case .scratch: "Scratch" }
    }
    var systemImage: String {
        switch self { case .yaml: "doc.text"; case .scratch: "puzzlepiece.extension" }
    }
}

struct StudioView: View {
    @EnvironmentObject var session: HubSession
    @State private var scenarios: [ScenarioMeta] = []
    @State private var selection: ScenarioMeta?
    @State private var loadedName: String?
    @State private var yamlText: String = ""
    @State private var validation: ValidationResult?
    @State private var saving = false
    @State private var validating = false
    @State private var error: String?
    @State private var dirty = false
    @State private var mode: StudioMode = .scratch

    var body: some View {
        #if os(macOS)
        HSplitView {
            list.frame(minWidth: 240)
            editor
        }
        .navigationTitle("Studio")
        .task { await loadList() }
        #else
        NavigationStack {
            list
                .navigationTitle("Studio")
                .navigationDestination(item: $selection) { meta in
                    editor.navigationTitle(meta.name)
                }
        }
        .task { await loadList() }
        #endif
    }

    private var list: some View {
        List(selection: $selection) {
            Section("Scénarios") {
                ForEach(scenarios) { meta in
                    VStack(alignment: .leading, spacing: 2) {
                        Text(meta.name).font(.body.monospaced())
                        Text("\(meta.size) o · \(modifiedDate(meta.modified))")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    .tag(meta)
                }
            }
            if let error {
                Section { Text(error).font(.caption).foregroundStyle(.red) }
            }
        }
        .refreshable { await loadList() }
        .onChange(of: selection) { _, new in
            guard let new else { return }
            Task { await loadDetail(name: new.name) }
        }
    }

    @ViewBuilder private var editor: some View {
        if let name = loadedName {
            VStack(spacing: 0) {
                modePicker
                Divider()
                switch mode {
                case .yaml:
                    VStack(spacing: 0) {
                        toolbar
                        Divider()
                        TextEditor(text: $yamlText)
                            .font(.system(.callout, design: .monospaced))
                            .scrollContentBackground(.hidden)
                            .background(Color.gray.opacity(0.05))
                            .onChange(of: yamlText) { _, _ in dirty = true }
                        if let validation { validationBanner(validation) }
                    }
                case .scratch:
                    ScratchEditorView(scenarioName: name)
                        .environmentObject(session)
                }
            }
        } else if selection != nil {
            ProgressView()
        } else {
            ContentUnavailableView("Aucune sélection", systemImage: "doc.text", description: Text("Choisis un scénario à gauche."))
        }
    }

    private var modePicker: some View {
        HStack {
            Picker("Mode", selection: $mode) {
                ForEach(StudioMode.allCases) { m in
                    Label(m.label, systemImage: m.systemImage).tag(m)
                }
            }
            .pickerStyle(.segmented)
            .frame(maxWidth: 280)
            Spacer()
        }
        .padding(8)
    }

    private var toolbar: some View {
        HStack(spacing: 8) {
            if dirty { Label("Modifié", systemImage: "pencil.circle").foregroundStyle(.orange) }
            Spacer()
            Button {
                Task { await validate() }
            } label: {
                Label(validating ? "…" : "Valider", systemImage: "checkmark.shield")
            }
            .disabled(validating)
            Button {
                Task { await save() }
            } label: {
                Label(saving ? "…" : "Enregistrer", systemImage: "square.and.arrow.down")
            }
            .buttonStyle(.borderedProminent)
            .disabled(saving || !dirty)
        }
        .padding(8)
    }

    private func validationBanner(_ v: ValidationResult) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Image(systemName: v.ok ? "checkmark.circle.fill" : "xmark.octagon.fill")
                Text(v.ok ? "YAML valide" : "Erreurs détectées").font(.subheadline.bold())
            }
            .foregroundStyle(v.ok ? .green : .red)
            ForEach(v.errors, id: \.self) { e in Text("• \(e)").font(.caption).foregroundStyle(.red) }
            ForEach(v.warnings, id: \.self) { w in Text("⚠︎ \(w)").font(.caption).foregroundStyle(.orange) }
            if !v.top_level_keys.isEmpty {
                Text("Clés: \(v.top_level_keys.joined(separator: ", "))").font(.caption2).foregroundStyle(.secondary)
            }
        }
        .padding(8)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(.thinMaterial)
    }

    private func loadList() async {
        do {
            scenarios = try await session.api.listScenarios()
            error = nil
        } catch { self.error = error.localizedDescription }
    }

    private func loadDetail(name: String) async {
        do {
            let detail = try await session.api.loadScenario(name: name)
            loadedName = detail.name
            yamlText = detail.yaml
            validation = nil
            dirty = false
            error = nil
        } catch { self.error = error.localizedDescription }
    }

    private func validate() async {
        guard let name = loadedName else { return }
        validating = true
        defer { validating = false }
        do {
            validation = try await session.api.validateScenario(name: name, yaml: yamlText)
        } catch let err { error = err.localizedDescription }
    }

    private func save() async {
        guard let name = loadedName else { return }
        saving = true
        defer { saving = false }
        do {
            validation = try await session.api.saveScenario(name: name, yaml: yamlText)
            dirty = false
        } catch let err { error = err.localizedDescription }
    }

    private func modifiedDate(_ epoch: Double) -> String {
        let date = Date(timeIntervalSince1970: epoch)
        let f = DateFormatter(); f.dateStyle = .short; f.timeStyle = .short
        return f.string(from: date)
    }
}
