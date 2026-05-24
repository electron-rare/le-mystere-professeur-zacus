import Foundation

/// v2 round-trip codec: BlocksDocument ↔ flat-nodes YAML.
///
/// v1 was a chain-centric layout that silently dropped slot child chains.
/// v2 emits every node as a flat list with `next:` and `slots:` references,
/// so any graph topology round-trips losslessly.
enum BlocksYAML {

    static let currentVersion = 2

    // MARK: encode

    static func encode(_ doc: BlocksDocument) -> String {
        var out = "blocks_studio_version: \(currentVersion)\nnodes:\n"
        if doc.nodes.isEmpty { out += "  []\n"; return out }
        for node in doc.nodes {
            out += "  - id: \"\(node.id.uuidString)\"\n"
            out += "    kind: \(node.kind.rawValue)\n"
            out += "    position: [\(Int(node.position.x)), \(Int(node.position.y))]\n"
            if let nextID = node.nextID {
                out += "    next: \"\(nextID.uuidString)\"\n"
            }
            if !node.params.isEmpty {
                out += "    params:\n"
                for key in node.params.keys.sorted() {
                    out += "      \(key): \(yamlScalar(node.params[key] ?? ""))\n"
                }
            }
            if !node.slots.isEmpty {
                out += "    slots:\n"
                for key in node.slots.keys.sorted() {
                    if let head = node.slots[key] {
                        out += "      \(key): \"\(head.uuidString)\"\n"
                    }
                }
            }
        }
        return out
    }

    private static func yamlScalar(_ value: String) -> String {
        if value.isEmpty { return "\"\"" }
        if value.contains("\n") {
            let lines = value.split(separator: "\n", omittingEmptySubsequences: false)
            return "|\n" + lines.map { "        \($0)" }.joined(separator: "\n")
        }
        // Always quote — keeps round-trip safe across YAML type inference.
        let escaped = value
            .replacingOccurrences(of: "\\", with: "\\\\")
            .replacingOccurrences(of: "\"", with: "\\\"")
        return "\"\(escaped)\""
    }

    // MARK: decode

    enum DecodeError: Error, LocalizedError {
        case notBlocksDocument
        case unsupportedVersion(Int)
        case malformed(String)
        var errorDescription: String? {
            switch self {
            case .notBlocksDocument:    return "YAML ne contient pas un document blocks_studio_version."
            case .unsupportedVersion(let v): return "blocks_studio_version \(v) non supporté (attendu: 1 ou 2)."
            case .malformed(let m):     return "YAML malformé: \(m)"
            }
        }
    }

    /// Single-purpose tolerant parser. Recognises v1 (legacy chains/sequence) and v2 (flat nodes).
    static func decode(_ text: String) throws -> BlocksDocument {
        guard text.contains("blocks_studio_version") else { throw DecodeError.notBlocksDocument }
        let version = parseVersion(text)
        switch version {
        case 1: return try decodeV1(text)
        case 2: return try decodeV2(text)
        default: throw DecodeError.unsupportedVersion(version)
        }
    }

    private static func parseVersion(_ text: String) -> Int {
        for raw in text.split(separator: "\n") {
            let line = String(raw).trimmingCharacters(in: .whitespaces)
            if line.hasPrefix("blocks_studio_version:") {
                let value = line.dropFirst("blocks_studio_version:".count).trimmingCharacters(in: .whitespaces)
                return Int(value) ?? 0
            }
        }
        return 0
    }

    // MARK: v2 parser

    private static func decodeV2(_ text: String) throws -> BlocksDocument {
        var doc = BlocksDocument()
        let lines = text.split(separator: "\n", omittingEmptySubsequences: false).map(String.init)
        var i = 0
        while i < lines.count, !lines[i].trimmingCharacters(in: .whitespaces).hasPrefix("nodes:") { i += 1 }
        i += 1
        var current: BlockNode?
        var inParams = false
        var inSlots = false
        func flush() { if let c = current { doc.nodes.append(c) }; current = nil; inParams = false; inSlots = false }
        while i < lines.count {
            let raw = lines[i]
            let line = raw.trimmingCharacters(in: .whitespaces)
            if line.hasPrefix("- id:") {
                flush()
                let idRaw = line.dropFirst(5).trimmingCharacters(in: .whitespaces).trimmingCharacters(in: CharacterSet(charactersIn: "\""))
                let id = UUID(uuidString: String(idRaw)) ?? UUID()
                current = BlockNode(id: id, kind: .sceneStart, position: .zero)
                inParams = false; inSlots = false
            } else if line.hasPrefix("kind:"), current != nil {
                let raw = line.dropFirst(5).trimmingCharacters(in: .whitespaces)
                if let kind = BlockKind(rawValue: String(raw)) { current?.kind = kind }
                inParams = false; inSlots = false
            } else if line.hasPrefix("position:"), current != nil {
                let nums = line.dropFirst(9).trimmingCharacters(in: .whitespaces)
                    .trimmingCharacters(in: CharacterSet(charactersIn: "[] "))
                    .split(separator: ",").compactMap { Double($0.trimmingCharacters(in: .whitespaces)) }
                if nums.count == 2 { current?.position = CGPoint(x: nums[0], y: nums[1]) }
                inParams = false; inSlots = false
            } else if line.hasPrefix("next:"), current != nil {
                let raw = line.dropFirst(5).trimmingCharacters(in: .whitespaces).trimmingCharacters(in: CharacterSet(charactersIn: "\""))
                current?.nextID = UUID(uuidString: String(raw))
                inParams = false; inSlots = false
            } else if line.hasPrefix("params:"), current != nil {
                inParams = true; inSlots = false
            } else if line.hasPrefix("slots:"), current != nil {
                inSlots = true; inParams = false
            } else if (inParams || inSlots), let colon = line.firstIndex(of: ":"), current != nil {
                let key = String(line[..<colon]).trimmingCharacters(in: .whitespaces)
                let value = String(line[line.index(after: colon)...]).trimmingCharacters(in: .whitespaces).trimmingCharacters(in: CharacterSet(charactersIn: "\""))
                if !key.isEmpty {
                    if inParams { current?.params[key] = value }
                    if inSlots, let uuid = UUID(uuidString: value) { current?.slots[key] = uuid }
                }
            }
            i += 1
        }
        flush()
        if doc.nodes.isEmpty { throw DecodeError.malformed("no nodes parsed") }
        return doc
    }

    // MARK: v1 legacy parser (kept for back-compat read; we re-save as v2)

    private static func decodeV1(_ text: String) throws -> BlocksDocument {
        var doc = BlocksDocument()
        let lines = text.split(separator: "\n", omittingEmptySubsequences: false).map(String.init)
        var i = 0
        while i < lines.count, !lines[i].trimmingCharacters(in: .whitespaces).hasPrefix("chains:") { i += 1 }
        i += 1
        while i < lines.count {
            let line = lines[i].trimmingCharacters(in: .whitespaces)
            if line.hasPrefix("- root:") {
                i += 1
                var rootPos = CGPoint.zero
                while i < lines.count, !lines[i].trimmingCharacters(in: .whitespaces).hasPrefix("sequence:") {
                    let l = lines[i].trimmingCharacters(in: .whitespaces)
                    if l.hasPrefix("position:") {
                        let nums = l.dropFirst(9).trimmingCharacters(in: .whitespaces)
                            .trimmingCharacters(in: CharacterSet(charactersIn: "[] "))
                            .split(separator: ",").compactMap { Double($0.trimmingCharacters(in: .whitespaces)) }
                        if nums.count == 2 { rootPos = CGPoint(x: nums[0], y: nums[1]) }
                    }
                    i += 1
                }
                i += 1
                var seq: [BlockNode] = []
                var currentParams: [String: String] = [:]
                var currentKind: BlockKind?
                var currentID: UUID?
                var inParams = false
                while i < lines.count {
                    let l = lines[i].trimmingCharacters(in: .whitespaces)
                    if l.hasPrefix("- root:") || lines[i].hasPrefix("  - root:") { break }
                    if l.hasPrefix("- kind:") {
                        if let k = currentKind {
                            seq.append(BlockNode(id: currentID ?? UUID(), kind: k, position: .zero, params: currentParams))
                        }
                        currentParams = [:]; currentID = nil; inParams = false
                        let kindRaw = l.dropFirst(7).trimmingCharacters(in: .whitespaces)
                        currentKind = BlockKind(rawValue: String(kindRaw))
                    } else if l.hasPrefix("id:") && currentKind != nil {
                        let raw = l.dropFirst(3).trimmingCharacters(in: .whitespaces).trimmingCharacters(in: CharacterSet(charactersIn: "\""))
                        currentID = UUID(uuidString: String(raw))
                    } else if l.hasPrefix("params:") {
                        inParams = true
                    } else if inParams, let colon = l.firstIndex(of: ":") {
                        let key = String(l[..<colon]).trimmingCharacters(in: .whitespaces)
                        let value = String(l[l.index(after: colon)...]).trimmingCharacters(in: .whitespaces).trimmingCharacters(in: CharacterSet(charactersIn: "\""))
                        if !key.isEmpty { currentParams[key] = value }
                    }
                    i += 1
                }
                if let k = currentKind {
                    seq.append(BlockNode(id: currentID ?? UUID(), kind: k, position: .zero, params: currentParams))
                }
                var y: CGFloat = rootPos.y
                let x: CGFloat = rootPos.x
                var prev: UUID?
                for var n in seq {
                    n.position = CGPoint(x: x, y: y)
                    y += estimatedHeight(of: n)
                    doc.nodes.append(n)
                    if let prev, let pIdx = doc.nodes.firstIndex(where: { $0.id == prev }) {
                        doc.nodes[pIdx].nextID = n.id
                    }
                    prev = n.id
                }
                continue
            }
            i += 1
        }
        return doc
    }
}
