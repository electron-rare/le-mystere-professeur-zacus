import Foundation
import SwiftUI

/// Static metadata for each BlockKind: label, parameter schema, default values.
struct BlockSpec {
    let kind: BlockKind
    let title: String
    let summary: String
    let params: [ParamSpec]
    /// Named child slots (Scratch C-shape). Empty for simple blocks.
    let slots: [SlotSpec]

    struct SlotSpec { let name: String; let label: String }

    init(kind: BlockKind, title: String, summary: String, params: [ParamSpec], slots: [SlotSpec] = []) {
        self.kind = kind
        self.title = title
        self.summary = summary
        self.params = params
        self.slots = slots
    }

    struct ParamSpec {
        let name: String
        let label: String
        let kind: ParamKind
        let placeholder: String
        let defaultValue: String
    }

    enum ParamKind: Equatable { case text, multiline, number, identifier, choice([String]) }

    func defaultParams() -> [String: String] {
        Dictionary(uniqueKeysWithValues: params.map { ($0.name, $0.defaultValue) })
    }
}

enum BlockCatalog {

    static let all: [BlockSpec] = [
        // Scene
        BlockSpec(kind: .sceneStart, title: "Début de scène", summary: "Marque le début d'une scène nommée.",
                  params: [.init(name: "id", label: "Identifiant", kind: .identifier, placeholder: "intro", defaultValue: "scene_id")]),
        BlockSpec(kind: .sceneEnd, title: "Fin de scène", summary: "Termine la scène courante.", params: []),
        BlockSpec(kind: .sceneGoto, title: "Aller à la scène", summary: "Saut inconditionnel.",
                  params: [.init(name: "target", label: "Cible", kind: .identifier, placeholder: "next_scene", defaultValue: "")]),
        BlockSpec(kind: .sceneBranch, title: "Branche conditionnelle", summary: "Si la condition est vraie, aller à A sinon B.",
                  params: [
                    .init(name: "condition", label: "Condition", kind: .text, placeholder: "score > 5", defaultValue: ""),
                    .init(name: "ifTrue", label: "Si vrai", kind: .identifier, placeholder: "scene_a", defaultValue: ""),
                    .init(name: "ifFalse", label: "Sinon", kind: .identifier, placeholder: "scene_b", defaultValue: "")
                  ]),

        // NPC
        BlockSpec(kind: .npcSay, title: "Zacus dit", summary: "Diffuse une réplique via TTS.",
                  params: [.init(name: "text", label: "Texte", kind: .multiline, placeholder: "Bonjour, je suis le professeur Zacus.", defaultValue: "")]),
        BlockSpec(kind: .npcWaitResponse, title: "Attendre une réponse", summary: "Bloque jusqu'à fin d'utterance.",
                  params: [.init(name: "timeout_s", label: "Timeout (s)", kind: .number, placeholder: "10", defaultValue: "10")]),
        BlockSpec(kind: .npcIntentMatch, title: "Si l'intent =", summary: "Match l'intent renvoyé par le NPC.",
                  params: [
                    .init(name: "intent", label: "Intent", kind: .identifier, placeholder: "yes", defaultValue: ""),
                    .init(name: "then", label: "Alors aller à", kind: .identifier, placeholder: "scene_x", defaultValue: "")
                  ]),

        // Hardware
        BlockSpec(kind: .hwServo, title: "Servo", summary: "Pose un angle sur un servo (MCPWM).",
                  params: [
                    .init(name: "channel", label: "Canal", kind: .number, placeholder: "0", defaultValue: "0"),
                    .init(name: "angle", label: "Angle (°)", kind: .number, placeholder: "90", defaultValue: "90")
                  ]),
        BlockSpec(kind: .hwReadQR, title: "Lire un QR", summary: "Attend un scan QR de la caméra.",
                  params: [.init(name: "expected", label: "Attendu", kind: .text, placeholder: "ZAC-A1", defaultValue: "")]),
        BlockSpec(kind: .hwLEDPattern, title: "LED — motif", summary: "Joue un motif LED nommé.",
                  params: [.init(name: "pattern", label: "Motif", kind: .choice(["rainbow","blink","fade","off"]), placeholder: "rainbow", defaultValue: "rainbow")]),
        BlockSpec(kind: .hwSoundPlay, title: "Jouer un son", summary: "Joue un asset audio du media manager.",
                  params: [.init(name: "asset", label: "Asset", kind: .identifier, placeholder: "sting_win", defaultValue: "")]),
        BlockSpec(kind: .hwAudioStop, title: "Stopper l'audio", summary: "Coupe le canal audio en cours.", params: []),
        BlockSpec(kind: .hwAudioVolume, title: "Volume audio", summary: "Règle le volume (0-100).",
                  params: [.init(name: "level", label: "Niveau", kind: .number, placeholder: "70", defaultValue: "70")]),

        BlockSpec(kind: .hwLCDText, title: "LCD — texte",
                  summary: "Affiche du texte (compatible LCD générique, BOX-3, M5Stack).",
                  params: [
                    .init(name: "line", label: "Ligne", kind: .number, placeholder: "0", defaultValue: "0"),
                    .init(name: "text", label: "Texte", kind: .text, placeholder: "Bienvenue", defaultValue: "")
                  ]),
        BlockSpec(kind: .hwLCDClear, title: "LCD — effacer", summary: "Vide l'écran.", params: []),
        BlockSpec(kind: .hwLCDImage, title: "LCD — image", summary: "Affiche un asset image plein écran.",
                  params: [.init(name: "asset", label: "Asset image", kind: .identifier, placeholder: "splash", defaultValue: "")]),
        BlockSpec(kind: .hwLCDTouchWait, title: "LCD — attendre tap",
                  summary: "Bloque jusqu'à tap sur une zone touch ou timeout (BOX-3, M5Core2).",
                  params: [
                    .init(name: "zone", label: "Zone", kind: .identifier, placeholder: "btn_yes", defaultValue: ""),
                    .init(name: "timeout_s", label: "Timeout (s)", kind: .number, placeholder: "30", defaultValue: "30")
                  ]),

        BlockSpec(kind: .hwBuzzerTone, title: "Buzzer — tone",
                  summary: "Émet un bip.",
                  params: [
                    .init(name: "freq", label: "Fréquence (Hz)", kind: .number, placeholder: "2000", defaultValue: "2000"),
                    .init(name: "ms", label: "Durée (ms)", kind: .number, placeholder: "120", defaultValue: "120")
                  ]),
        BlockSpec(kind: .hwRelay, title: "Relais",
                  summary: "Bascule un relais (ouvre une porte, etc.).",
                  params: [
                    .init(name: "channel", label: "Canal", kind: .number, placeholder: "0", defaultValue: "0"),
                    .init(name: "state", label: "État", kind: .choice(["on","off","pulse"]), placeholder: "pulse", defaultValue: "pulse")
                  ]),
        BlockSpec(kind: .hwSensorRead, title: "Lire capteur",
                  summary: "Lit un capteur analogique et stocke dans une variable.",
                  params: [
                    .init(name: "pin", label: "Pin / canal", kind: .identifier, placeholder: "A0", defaultValue: "A0"),
                    .init(name: "var", label: "Variable", kind: .identifier, placeholder: "lecture", defaultValue: "lecture")
                  ]),
        BlockSpec(kind: .hwButtonWait, title: "Attendre bouton",
                  summary: "Bloque jusqu'à appui sur un bouton GPIO.",
                  params: [
                    .init(name: "button", label: "Bouton", kind: .identifier, placeholder: "btn_main", defaultValue: "btn_main"),
                    .init(name: "timeout_s", label: "Timeout (s)", kind: .number, placeholder: "0", defaultValue: "0")
                  ]),

        // ESP-NOW
        BlockSpec(kind: .espnowRegisterPeer, title: "ESP-NOW — déclarer peer",
                  summary: "Enregistre un peer par MAC.",
                  params: [
                    .init(name: "alias", label: "Alias", kind: .identifier, placeholder: "annexe1", defaultValue: ""),
                    .init(name: "mac", label: "MAC", kind: .text, placeholder: "AA:BB:CC:DD:EE:FF", defaultValue: "")
                  ]),
        BlockSpec(kind: .espnowSend, title: "ESP-NOW — envoyer",
                  summary: "Envoie une commande à un peer.",
                  params: [
                    .init(name: "peer", label: "Peer (alias)", kind: .identifier, placeholder: "annexe1", defaultValue: ""),
                    .init(name: "command", label: "Commande", kind: .text, placeholder: "open_door", defaultValue: "")
                  ]),
        BlockSpec(kind: .espnowBroadcast, title: "ESP-NOW — broadcast",
                  summary: "Envoie une commande à tous les peers.",
                  params: [.init(name: "command", label: "Commande", kind: .text, placeholder: "reset", defaultValue: "")]),
        BlockSpec(kind: .espnowWait, title: "ESP-NOW — attendre",
                  summary: "Bloque jusqu'à réception d'une commande.",
                  params: [
                    .init(name: "command", label: "Commande attendue", kind: .text, placeholder: "ready", defaultValue: ""),
                    .init(name: "timeout_s", label: "Timeout (s)", kind: .number, placeholder: "10", defaultValue: "10")
                  ]),

        // BOX-3
        BlockSpec(kind: .boxIMUShake, title: "BOX-3 — secouer",
                  summary: "Attend une secousse détectée par l'IMU.",
                  params: [
                    .init(name: "threshold", label: "Seuil (g)", kind: .number, placeholder: "1.5", defaultValue: "1.5"),
                    .init(name: "timeout_s", label: "Timeout (s)", kind: .number, placeholder: "10", defaultValue: "10")
                  ]),
        BlockSpec(kind: .boxIRSend, title: "BOX-3 — IR send",
                  summary: "Envoie un code IR (NEC, etc.).",
                  params: [
                    .init(name: "protocol", label: "Protocole", kind: .choice(["NEC","RC5","SONY","RAW"]), placeholder: "NEC", defaultValue: "NEC"),
                    .init(name: "code", label: "Code (hex)", kind: .identifier, placeholder: "0x20DF10EF", defaultValue: "")
                  ]),

        // M5
        BlockSpec(kind: .m5Beep, title: "M5 — bip",
                  summary: "Bip via le buzzer M5 (Core2/StickC).",
                  params: [
                    .init(name: "freq", label: "Fréquence (Hz)", kind: .number, placeholder: "4000", defaultValue: "4000"),
                    .init(name: "ms", label: "Durée (ms)", kind: .number, placeholder: "200", defaultValue: "200")
                  ]),
        BlockSpec(kind: .m5LCDText, title: "M5 — texte LCD",
                  summary: "Affiche du texte sur l'écran M5 avec couleur.",
                  params: [
                    .init(name: "text", label: "Texte", kind: .text, placeholder: "Hello", defaultValue: ""),
                    .init(name: "color", label: "Couleur", kind: .choice(["white","yellow","red","green","blue","cyan","magenta"]), placeholder: "white", defaultValue: "white"),
                    .init(name: "size", label: "Taille", kind: .number, placeholder: "2", defaultValue: "2")
                  ]),
        BlockSpec(kind: .m5ButtonAB, title: "M5 — attendre bouton",
                  summary: "Attend l'appui sur A, B (ou C sur Core2).",
                  params: [
                    .init(name: "button", label: "Bouton", kind: .choice(["A","B","C","any"]), placeholder: "A", defaultValue: "A"),
                    .init(name: "timeout_s", label: "Timeout (s)", kind: .number, placeholder: "0", defaultValue: "0")
                  ]),
        BlockSpec(kind: .m5RGBLed, title: "M5 — LED RGB",
                  summary: "Allume la LED RGB (StickC) en couleur hex.",
                  params: [.init(name: "color", label: "Couleur (hex)", kind: .identifier, placeholder: "#FF8800", defaultValue: "#FF8800")]),
        BlockSpec(kind: .m5IMUShake, title: "M5 — secouer",
                  summary: "Attend une secousse (MPU6886).",
                  params: [
                    .init(name: "threshold", label: "Seuil (g)", kind: .number, placeholder: "1.5", defaultValue: "1.5"),
                    .init(name: "timeout_s", label: "Timeout (s)", kind: .number, placeholder: "10", defaultValue: "10")
                  ]),

        // PLIP
        BlockSpec(kind: .plipRing, title: "PLIP — sonner",
                  summary: "Déclenche la sonnerie du téléphone rétro.",
                  params: [.init(name: "duration_s", label: "Durée (s)", kind: .number, placeholder: "3", defaultValue: "3")]),
        BlockSpec(kind: .plipPickupWait, title: "PLIP — attendre décroché",
                  summary: "Bloque jusqu'à décroché du combiné.",
                  params: [.init(name: "timeout_s", label: "Timeout (s)", kind: .number, placeholder: "30", defaultValue: "30")]),

        // Logic
        BlockSpec(kind: .logicIf, title: "Si …", summary: "Exécute la chaîne attachée seulement si vrai.",
                  params: [.init(name: "condition", label: "Condition", kind: .text, placeholder: "score > 0", defaultValue: "")],
                  slots: [.init(name: "body", label: "Alors"), .init(name: "else", label: "Sinon")]),
        BlockSpec(kind: .logicTimer, title: "Timer", summary: "Attend N secondes.",
                  params: [.init(name: "seconds", label: "Secondes", kind: .number, placeholder: "5", defaultValue: "5")]),
        BlockSpec(kind: .logicScore, title: "Score", summary: "Ajoute N points (négatif pour retirer).",
                  params: [.init(name: "delta", label: "Delta", kind: .number, placeholder: "1", defaultValue: "1")]),
        BlockSpec(kind: .logicSetVar, title: "Variable :=", summary: "Définit une variable de scénario.",
                  params: [
                    .init(name: "name", label: "Nom", kind: .identifier, placeholder: "key", defaultValue: ""),
                    .init(name: "value", label: "Valeur", kind: .text, placeholder: "42", defaultValue: "")
                  ]),
    ]

    static let byKind: [BlockKind: BlockSpec] = Dictionary(uniqueKeysWithValues: all.map { ($0.kind, $0) })

    static func spec(_ kind: BlockKind) -> BlockSpec { byKind[kind]! }

    static func byCategory() -> [(BlockCategory, [BlockSpec])] {
        BlockCategory.allCases.map { cat in (cat, all.filter { $0.kind.category == cat }) }
    }
}
