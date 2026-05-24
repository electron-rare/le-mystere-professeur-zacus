import Foundation
import SwiftUI

// MARK: - Categories

enum BlockCategory: String, CaseIterable, Identifiable, Codable {
    case scene, npc, audio, lcd, hardware, espnow, box3, m5, plip, logic
    var id: String { rawValue }

    var label: String {
        switch self {
        case .scene:    return "Scènes"
        case .npc:      return "NPC (Zacus)"
        case .audio:    return "Audio"
        case .lcd:      return "Affichage LCD"
        case .hardware: return "Hardware ESP32"
        case .espnow:   return "ESP-NOW"
        case .box3:     return "ESP32-S3-BOX-3"
        case .m5:       return "M5Stack / M5StickC"
        case .plip:     return "PLIP (téléphone)"
        case .logic:    return "Logique"
        }
    }

    var color: Color {
        switch self {
        case .scene:    return Color(red: 0.32, green: 0.55, blue: 0.95)
        case .npc:      return Color(red: 0.85, green: 0.36, blue: 0.45)
        case .audio:    return Color(red: 0.65, green: 0.45, blue: 0.85)
        case .lcd:      return Color(red: 0.20, green: 0.55, blue: 0.75)
        case .hardware: return Color(red: 0.18, green: 0.68, blue: 0.55)
        case .espnow:   return Color(red: 0.95, green: 0.50, blue: 0.20)
        case .box3:     return Color(red: 0.55, green: 0.35, blue: 0.20)
        case .m5:       return Color(red: 0.30, green: 0.45, blue: 0.65)
        case .plip:     return Color(red: 0.78, green: 0.30, blue: 0.55)
        case .logic:    return Color(red: 0.95, green: 0.65, blue: 0.20)
        }
    }
}

// MARK: - Kinds (15 prioritized)

enum BlockKind: String, CaseIterable, Codable {
    // Scene
    case sceneStart, sceneEnd, sceneGoto, sceneBranch
    // NPC
    case npcSay, npcWaitResponse, npcIntentMatch
    // Audio
    case hwSoundPlay, hwAudioStop, hwAudioVolume
    // LCD
    case hwLCDText, hwLCDClear, hwLCDImage, hwLCDTouchWait
    // Hardware
    case hwServo, hwReadQR, hwLEDPattern, hwBuzzerTone, hwRelay, hwSensorRead, hwButtonWait
    // ESP-NOW
    case espnowRegisterPeer, espnowSend, espnowBroadcast, espnowWait
    // ESP32-S3-BOX-3
    case boxIMUShake, boxIRSend
    // M5Stack / M5StickC
    case m5Beep, m5LCDText, m5ButtonAB, m5RGBLed, m5IMUShake
    // PLIP
    case plipRing, plipPickupWait
    // Logic
    case logicIf, logicTimer, logicScore, logicSetVar

    var category: BlockCategory {
        switch self {
        case .sceneStart, .sceneEnd, .sceneGoto, .sceneBranch: return .scene
        case .npcSay, .npcWaitResponse, .npcIntentMatch: return .npc
        case .hwSoundPlay, .hwAudioStop, .hwAudioVolume: return .audio
        case .hwLCDText, .hwLCDClear, .hwLCDImage, .hwLCDTouchWait: return .lcd
        case .hwServo, .hwReadQR, .hwLEDPattern, .hwBuzzerTone, .hwRelay, .hwSensorRead, .hwButtonWait: return .hardware
        case .espnowRegisterPeer, .espnowSend, .espnowBroadcast, .espnowWait: return .espnow
        case .boxIMUShake, .boxIRSend: return .box3
        case .m5Beep, .m5LCDText, .m5ButtonAB, .m5RGBLed, .m5IMUShake: return .m5
        case .plipRing, .plipPickupWait: return .plip
        case .logicIf, .logicTimer, .logicScore, .logicSetVar: return .logic
        }
    }
}

// MARK: - Block instance

struct BlockNode: Identifiable, Codable, Equatable {
    let id: UUID
    var kind: BlockKind
    var position: CGPoint
    var params: [String: String]
    /// id of the block snapped under this one (Scratch-style vertical chain).
    var nextID: UUID?
    /// Child slot heads, keyed by slot name (e.g. "body", "else"). The slot's
    /// chain is the linear list reachable via .nextID from this head.
    var slots: [String: UUID]

    init(id: UUID = UUID(), kind: BlockKind, position: CGPoint, params: [String: String] = [:], nextID: UUID? = nil, slots: [String: UUID] = [:]) {
        self.id = id
        self.kind = kind
        self.position = position
        self.params = params
        self.nextID = nextID
        self.slots = slots
    }
}

// MARK: - Document

struct BlocksDocument: Codable, Equatable {
    var nodes: [BlockNode] = []
    /// Roots (no parent pointing to them) ordered as they appear on canvas.
    func roots() -> [BlockNode] {
        var referenced = Set(nodes.compactMap(\.nextID))
        for n in nodes { for s in n.slots.values { referenced.insert(s) } }
        return nodes.filter { !referenced.contains($0.id) }
    }
    func node(_ id: UUID) -> BlockNode? { nodes.first(where: { $0.id == id }) }
    mutating func update(_ node: BlockNode) {
        if let idx = nodes.firstIndex(where: { $0.id == node.id }) {
            nodes[idx] = node
        }
    }
    mutating func append(_ node: BlockNode) { nodes.append(node) }
    mutating func remove(_ id: UUID) {
        // detach from any chain
        for i in nodes.indices where nodes[i].nextID == id {
            nodes[i].nextID = nil
        }
        // detach from any slot
        for i in nodes.indices {
            nodes[i].slots = nodes[i].slots.filter { $0.value != id }
        }
        nodes.removeAll { $0.id == id }
    }
    /// Walk a chain starting at `start`, returning ordered nodes.
    func chain(from start: UUID) -> [BlockNode] {
        var out: [BlockNode] = []
        var cur: UUID? = start
        var seen = Set<UUID>()
        while let id = cur, !seen.contains(id), let n = node(id) {
            out.append(n)
            seen.insert(id)
            cur = n.nextID
        }
        return out
    }
}
