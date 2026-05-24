import SwiftUI

struct BlocksEditorView: View {
    @EnvironmentObject var session: HubSession
    let scenarioName: String

    @State private var document = BlocksDocument()
    @State private var selection: Set<UUID> = []
    @State private var dirty = false
    @State private var saving = false
    @State private var compiling = false
    @State private var message: String?
    @State private var error: String?
    @State private var zoom: CGFloat = 1.0
    @State private var liveZoom: CGFloat = 1.0
    @State private var undoStack: [BlocksDocument] = []
    @State private var redoStack: [BlocksDocument] = []

    var body: some View {
        VStack(spacing: 0) {
            toolbar
            Divider()
            HStack(spacing: 0) {
                BlockPaletteView(onAdd: addBlock)
                Divider()
                ZStack {
                    ScrollView([.horizontal, .vertical]) {
                        BlockCanvasView(
                            document: $document,
                            selection: $selection,
                            zoom: $zoom,
                            onMutate: { _ in dirty = true }
                        )
                        .frame(minWidth: 1600, minHeight: 1100)
                    }
                    .background(Color.gray.opacity(0.04))
                    if document.nodes.isEmpty {
                        ContentUnavailableView("Toile vide",
                                               systemImage: "rectangle.dashed",
                                               description: Text("Glisse un bloc depuis la palette à gauche, ou clique le + pour l'ajouter."))
                    }
                }
                #if os(macOS)
                .gesture(
                    MagnificationGesture()
                        .onChanged { value in
                            liveZoom = max(0.4, min(2.0, value * zoom))
                            zoom = liveZoom
                        }
                        .onEnded { _ in liveZoom = zoom }
                )
                #endif
            }
        }
        .navigationTitle("Blocks · \(scenarioName)")
        .onAppear {
            UndoBroker.shared.onSnapshot = { snap in
                undoStack.append(snap)
                if undoStack.count > 100 { undoStack.removeFirst(undoStack.count - 100) }
                redoStack.removeAll()
            }
        }
        .onDisappear { UndoBroker.shared.onSnapshot = nil }
        .task { await load() }
        .focusable()
        #if os(macOS)
        .onKeyPress(keys: [.delete, .init("\u{7F}")]) { _ in
            deleteSelection()
            return .handled
        }
        #endif
        .toolbar { toolbarItems }
    }

    @ToolbarContentBuilder private var toolbarItems: some ToolbarContent {
        ToolbarItemGroup(placement: .primaryAction) {
            Button { undo() } label: { Image(systemName: "arrow.uturn.backward") }
                .keyboardShortcut("z", modifiers: .command)
                .disabled(undoStack.isEmpty)
            Button { redo() } label: { Image(systemName: "arrow.uturn.forward") }
                .keyboardShortcut("z", modifiers: [.command, .shift])
                .disabled(redoStack.isEmpty)
            Button { zoom = max(0.4, zoom - 0.1) } label: { Image(systemName: "minus.magnifyingglass") }
                .keyboardShortcut("-", modifiers: .command)
            Text("\(Int(zoom*100))%").font(.caption.monospacedDigit()).frame(width: 44)
            Button { zoom = min(2.0, zoom + 0.1) } label: { Image(systemName: "plus.magnifyingglass") }
                .keyboardShortcut("=", modifiers: .command)
            Button { zoom = 1.0 } label: { Image(systemName: "1.magnifyingglass") }
                .keyboardShortcut("0", modifiers: .command)
        }
    }

    private var toolbar: some View {
        HStack(spacing: 8) {
            Text("Scénario: \(scenarioName)").font(.caption.monospaced()).foregroundStyle(.secondary)
            if dirty { Label("Modifié", systemImage: "pencil.circle").foregroundStyle(.orange) }
            if !selection.isEmpty {
                Text("· \(selection.count) sélectionné(s)").font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
            if let message { Text(message).font(.caption).foregroundStyle(.green) }
            if let error { Text(error).font(.caption).foregroundStyle(.red).lineLimit(1) }
            Button {
                Task { await compile() }
            } label: {
                Label(compiling ? "…" : "Compiler en IR", systemImage: "hammer")
            }
            .disabled(compiling || document.nodes.isEmpty)
            Button {
                snapshotForUndo()
                document = BlocksDocument(); selection.removeAll(); dirty = true
            } label: { Label("Vider", systemImage: "trash") }
            Button {
                Task { await save() }
            } label: { Label(saving ? "…" : "Enregistrer", systemImage: "square.and.arrow.down") }
            .buttonStyle(.borderedProminent)
            .disabled(saving || !dirty)
        }
        .padding(8)
    }

    // MARK: actions

    private func addBlock(_ kind: BlockKind) {
        snapshotForUndo()
        let spec = BlockCatalog.spec(kind)
        let pos = nextSpawnPosition()
        let node = BlockNode(kind: kind, position: pos, params: spec.defaultParams())
        document.append(node)
        selection = [node.id]
        dirty = true
    }

    private func nextSpawnPosition() -> CGPoint {
        let baseX: CGFloat = 80
        let baseY: CGFloat = 60
        let cols = document.nodes.count / 8
        return CGPoint(x: baseX + CGFloat(cols) * (blockWidth + 40),
                       y: baseY + CGFloat(document.nodes.count % 8) * 80)
    }

    private func deleteSelection() {
        guard !selection.isEmpty else { return }
        snapshotForUndo()
        for id in selection { document.remove(id) }
        selection.removeAll()
        dirty = true
    }

    // MARK: undo

    private func snapshotForUndo() {
        undoStack.append(document)
        if undoStack.count > 100 { undoStack.removeFirst(undoStack.count - 100) }
        redoStack.removeAll()
    }

    private func undo() {
        guard let prev = undoStack.popLast() else { return }
        redoStack.append(document)
        document = prev
        dirty = true
    }

    private func redo() {
        guard let next = redoStack.popLast() else { return }
        undoStack.append(document)
        document = next
        dirty = true
    }

    // MARK: gateway

    private func load() async {
        do {
            let detail = try await session.api.loadScenario(name: scenarioName)
            if detail.yaml.contains("blocks_studio_version") {
                document = try BlocksYAML.decode(detail.yaml)
            } else {
                document = BlocksDocument()
                message = "Scénario non blocky — éditeur vide. Sauvegarder l'écrasera."
            }
            dirty = false
            undoStack.removeAll(); redoStack.removeAll()
            error = nil
        } catch let err {
            error = err.localizedDescription
        }
    }

    private func save() async {
        saving = true; defer { saving = false }
        let yaml = BlocksYAML.encode(document)
        do {
            _ = try await session.api.saveScenario(name: scenarioName, yaml: yaml)
            dirty = false
            error = nil
            message = "Enregistré (\(document.nodes.count) blocs)"
        } catch let err {
            error = err.localizedDescription
            message = nil
        }
    }

    private func compile() async {
        compiling = true; defer { compiling = false }
        do {
            let resp = try await session.api.compileScenario(name: scenarioName)
            message = "Compilé: \(resp.steps_count) steps, entry=\(resp.entry_step_id ?? "—")"
            error = nil
        } catch let err {
            error = "Compile: \(err.localizedDescription)"
            message = nil
        }
    }
}
