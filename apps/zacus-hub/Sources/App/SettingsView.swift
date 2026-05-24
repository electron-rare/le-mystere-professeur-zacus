import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var session: HubSession
    @Environment(\.dismiss) private var dismiss
    @State private var baseURL: String = ""
    @State private var token: String = ""

    var body: some View {
        NavigationStack {
            Form {
                Section("Gateway") {
                    TextField("URL de base", text: $baseURL)
                        .textContentType(.URL)
                        .autocorrectionDisabled()
                        #if os(iOS)
                        .textInputAutocapitalization(.never)
                        .keyboardType(.URL)
                        #endif
                    SecureField("Token bearer", text: $token)
                        .textContentType(.password)
                }

                Section("État") {
                    Label(authLabel, systemImage: authIcon).foregroundStyle(authColor)
                    if let err = session.lastError { Text(err).font(.caption).foregroundStyle(.secondary) }
                }
            }
            .navigationTitle("Réglages")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Annuler") { dismiss() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Enregistrer") {
                        session.updateConfig(HubConfig(baseURL: baseURL, token: token))
                        dismiss()
                    }
                }
            }
        }
        .onAppear {
            baseURL = session.config.baseURL
            token = session.config.token
        }
        #if os(macOS)
        .frame(minWidth: 480, minHeight: 320)
        #endif
    }

    private var authLabel: String {
        switch session.authStatus {
        case .unknown: return "Non vérifié"
        case .ok:      return "Authentifié"
        case .failed:  return "Échec d'authentification"
        }
    }
    private var authIcon: String {
        switch session.authStatus {
        case .unknown: return "questionmark.circle"
        case .ok:      return "checkmark.circle.fill"
        case .failed:  return "exclamationmark.triangle.fill"
        }
    }
    private var authColor: Color {
        switch session.authStatus {
        case .unknown: return .secondary
        case .ok:      return .green
        case .failed:  return .red
        }
    }
}
