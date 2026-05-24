import SwiftUI

enum HubMode: String, CaseIterable, Identifiable {
    case gameMaster, companion, studio
    var id: String { rawValue }
    var label: String {
        switch self {
        case .gameMaster: return "Game-master"
        case .companion: return "Companion"
        case .studio:    return "Studio"
        }
    }
    var systemImage: String {
        switch self {
        case .gameMaster: return "gauge.with.dots.needle.bottom.50percent"
        case .companion:  return "waveform.and.mic"
        case .studio:     return "doc.text.below.ecg"
        }
    }
}

struct RootView: View {
    @EnvironmentObject var session: HubSession
    @State private var mode: HubMode = .gameMaster
    @State private var showSettings = false

    var body: some View {
        Group {
            #if os(macOS)
            NavigationSplitView {
                List(HubMode.allCases, selection: Binding($mode)) { item in
                    Label(item.label, systemImage: item.systemImage).tag(item)
                }
                .navigationTitle("Zacus Hub")
            } detail: {
                content
            }
            #else
            TabView(selection: $mode) {
                ForEach(HubMode.allCases) { item in
                    contentView(for: item)
                        .tabItem { Label(item.label, systemImage: item.systemImage) }
                        .tag(item)
                }
            }
            #endif
        }
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button { showSettings = true } label: { Image(systemName: "gearshape") }
            }
        }
        .sheet(isPresented: $showSettings) { SettingsView() }
    }

    @ViewBuilder private var content: some View { contentView(for: mode) }

    @ViewBuilder private func contentView(for mode: HubMode) -> some View {
        switch mode {
        case .gameMaster: GameMasterView()
        case .companion:  CompanionView()
        case .studio:     StudioView()
        }
    }
}

private extension Binding where Value == HubMode {
    init(_ source: Binding<HubMode>) { self.init(get: { source.wrappedValue }, set: { source.wrappedValue = $0 }) }
}
