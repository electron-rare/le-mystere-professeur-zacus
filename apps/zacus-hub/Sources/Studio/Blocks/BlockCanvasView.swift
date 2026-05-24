import SwiftUI

#if canImport(UniformTypeIdentifiers)
import UniformTypeIdentifiers
#endif

struct BlockCanvasView: View {
    @Binding var document: BlocksDocument
    @Binding var selection: Set<UUID>
    @Binding var zoom: CGFloat
    var onMutate: (BlocksDocument) -> Void = { _ in }

    @State private var dragOffsets: [UUID: CGSize] = [:]
    @State private var hoveredSlot: SlotTarget?

    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .topLeading) {
                Color.gray.opacity(0.06)
                    .onTapGesture { selection.removeAll() }
                gridBackground(size: geo.size)
                ForEach(document.nodes) { node in
                    blockBinding(node).map { binding in
                        BlockView(
                            node: binding,
                            isSelected: selection.contains(node.id),
                            onSelect: { toggleSelect(node.id) },
                            onDelete: { snapshot(); document.remove(node.id); selection.remove(node.id); onMutate(document) }
                        )
                        .position(positionOf(node))
                        .gesture(dragGesture(for: node))
                    }
                }
                connectorLayer
                if let hoveredSlot { slotHighlight(hoveredSlot) }
            }
            .frame(minWidth: max(geo.size.width, contentExtent.width) * zoom,
                   minHeight: max(geo.size.height, contentExtent.height) * zoom,
                   alignment: .topLeading)
            .scaleEffect(zoom, anchor: .topLeading)
            .animation(.easeOut(duration: 0.12), value: zoom)
            #if os(macOS)
            .dropDestination(for: PaletteDrop.self) { drops, location in
                guard let drop = drops.first else { return false }
                let pos = CGPoint(x: location.x / zoom - blockWidth/2, y: location.y / zoom - 20)
                snapshot()
                let spec = BlockCatalog.spec(drop.kind)
                let node = BlockNode(kind: drop.kind, position: pos, params: spec.defaultParams())
                document.append(node)
                onMutate(document)
                selection = [node.id]
                return true
            } isTargeted: { _ in }
            #endif
        }
    }

    // MARK: helpers

    private func toggleSelect(_ id: UUID) {
        #if os(macOS)
        let shift = NSEvent.modifierFlags.contains(.shift)
        #else
        let shift = false
        #endif
        if shift {
            if selection.contains(id) { selection.remove(id) } else { selection.insert(id) }
        } else {
            selection = [id]
        }
    }

    private func snapshot() {
        UndoBroker.shared.push(document)
    }

    private func positionOf(_ node: BlockNode) -> CGPoint {
        let offset = dragOffsets[node.id] ?? .zero
        let h = estimatedHeight(of: node)
        return CGPoint(x: node.position.x + blockWidth/2 + offset.width,
                       y: node.position.y + h/2 + offset.height)
    }

    private func blockBinding(_ node: BlockNode) -> Binding<BlockNode>? {
        guard let idx = document.nodes.firstIndex(where: { $0.id == node.id }) else { return nil }
        return $document.nodes[idx]
    }

    private var contentExtent: CGSize {
        let xs = document.nodes.map { $0.position.x + blockWidth + 80 }
        let ys = document.nodes.map { $0.position.y + estimatedHeight(of: $0) + 80 }
        return CGSize(width: xs.max() ?? 600, height: ys.max() ?? 400)
    }

    // MARK: drag + snap

    private func dragGesture(for node: BlockNode) -> some Gesture {
        DragGesture(minimumDistance: 1, coordinateSpace: .local)
            .onChanged { value in
                dragOffsets[node.id] = value.translation
                if !selection.contains(node.id) { selection = [node.id] }
                // live preview snap target
                let liveTop = CGPoint(x: node.position.x + value.translation.width,
                                      y: node.position.y + value.translation.height)
                hoveredSlot = findSlotTarget(droppingTop: liveTop, dragged: node.id)
            }
            .onEnded { value in
                dragOffsets[node.id] = nil
                hoveredSlot = nil
                guard let idx = document.nodes.firstIndex(where: { $0.id == node.id }) else { return }
                snapshot()
                // detach this block from any parent + slot
                detach(node.id)
                var moved = document.nodes[idx]
                moved.position.x += value.translation.width
                moved.position.y += value.translation.height
                document.nodes[idx] = moved
                snap(node: moved)
                onMutate(document)
            }
    }

    private func detach(_ id: UUID) {
        for i in document.nodes.indices where document.nodes[i].nextID == id {
            document.nodes[i].nextID = nil
        }
        for i in document.nodes.indices {
            document.nodes[i].slots = document.nodes[i].slots.filter { $0.value != id }
        }
    }

    private struct SlotTarget {
        let parentID: UUID
        let slotName: String?   // nil = bottom (regular next)
        let anchor: CGPoint     // top-left where moved block should land
    }

    private func findSlotTarget(droppingTop top: CGPoint, dragged: UUID) -> SlotTarget? {
        let chainIDs = Set(document.chain(from: dragged).map(\.id))
        var best: (SlotTarget, CGFloat)?
        for other in document.nodes where other.id != dragged && !chainIDs.contains(other.id) {
            let spec = BlockCatalog.spec(other.kind)
            // (a) regular bottom snap
            let bottom = CGPoint(x: other.position.x, y: other.position.y + estimatedHeight(of: other))
            let dx = abs(bottom.x - top.x), dy = abs(bottom.y - top.y)
            if dx < snapDistance && dy < snapDistance {
                let cand = SlotTarget(parentID: other.id, slotName: nil, anchor: bottom)
                if best == nil || (dx+dy) < best!.1 { best = (cand, dx+dy) }
            }
            // (b) slot mouths — approximate vertical placement after params
            var slotY = other.position.y + 38
            for p in spec.params { slotY += (p.kind == .multiline ? 78 : 38) }
            for slot in spec.slots {
                let mouth = CGPoint(x: other.position.x + 22, y: slotY + 34)
                let sdx = abs(mouth.x - top.x), sdy = abs(mouth.y - top.y)
                if sdx < snapDistance*1.5 && sdy < snapDistance*1.5 {
                    let cand = SlotTarget(parentID: other.id, slotName: slot.name, anchor: mouth)
                    if best == nil || (sdx+sdy) < best!.1 { best = (cand, sdx+sdy) }
                }
                slotY += 34
            }
        }
        return best?.0
    }

    private func snap(node moved: BlockNode) {
        guard let target = findSlotTarget(droppingTop: moved.position, dragged: moved.id) else { return }
        guard let mIdx = document.nodes.firstIndex(where: { $0.id == moved.id }),
              let pIdx = document.nodes.firstIndex(where: { $0.id == target.parentID }) else { return }
        if let slotName = target.slotName {
            // slot snap: place at the head of the slot chain, push prior head after our tail
            let prior = document.nodes[pIdx].slots[slotName]
            document.nodes[mIdx].position = target.anchor
            document.nodes[pIdx].slots[slotName] = moved.id
            if let prior, prior != moved.id {
                let tail = document.chain(from: moved.id).last ?? moved
                if let tIdx = document.nodes.firstIndex(where: { $0.id == tail.id }) {
                    document.nodes[tIdx].nextID = prior
                }
            }
        } else {
            // regular bottom chain
            document.nodes[mIdx].position = target.anchor
            let prior = document.nodes[pIdx].nextID
            document.nodes[pIdx].nextID = moved.id
            if let prior, prior != moved.id {
                let tail = document.chain(from: moved.id).last ?? moved
                if let tIdx = document.nodes.firstIndex(where: { $0.id == tail.id }) {
                    document.nodes[tIdx].nextID = prior
                }
            }
        }
        cascadePositions(from: target.parentID)
    }

    private func cascadePositions(from rootID: UUID) {
        guard let root = document.node(rootID) else { return }
        // Cascade vertical chain
        var cur: UUID? = root.nextID
        var y: CGFloat = root.position.y + estimatedHeight(of: root)
        let x: CGFloat = root.position.x
        var seen = Set<UUID>()
        while let id = cur, !seen.contains(id), let idx = document.nodes.firstIndex(where: { $0.id == id }) {
            seen.insert(id)
            document.nodes[idx].position = CGPoint(x: x, y: y)
            y += estimatedHeight(of: document.nodes[idx])
            cur = document.nodes[idx].nextID
        }
        // Cascade slot contents
        let spec = BlockCatalog.spec(root.kind)
        var slotY = root.position.y + 38
        for p in spec.params { slotY += (p.kind == .multiline ? 78 : 38) }
        for slot in spec.slots {
            if let head = root.slots[slot.name] {
                cascadePositions(from: head)
                if let hIdx = document.nodes.firstIndex(where: { $0.id == head }) {
                    document.nodes[hIdx].position = CGPoint(x: root.position.x + 20, y: slotY + 36)
                    cascadePositions(from: head) // re-flow with corrected head
                }
            }
            slotY += 34
        }
    }

    // MARK: visuals

    @ViewBuilder private var connectorLayer: some View {
        Canvas { context, _ in
            for node in document.nodes {
                if let nextID = node.nextID, let next = document.node(nextID) {
                    drawWire(context: context, from: node, to: next, color: .white.opacity(0.7))
                }
                for (_, head) in node.slots {
                    if let h = document.node(head) {
                        drawWire(context: context, from: node, to: h, color: .yellow.opacity(0.8))
                    }
                }
            }
        }
        .allowsHitTesting(false)
    }

    private func drawWire(context: GraphicsContext, from: BlockNode, to: BlockNode, color: Color) {
        let a = CGPoint(x: from.position.x + 16, y: from.position.y + estimatedHeight(of: from))
        let b = CGPoint(x: to.position.x + 16, y: to.position.y)
        var path = Path()
        path.move(to: a); path.addLine(to: b)
        context.stroke(path, with: .color(color), lineWidth: 3)
    }

    @ViewBuilder private func slotHighlight(_ t: SlotTarget) -> some View {
        RoundedRectangle(cornerRadius: 6)
            .stroke(.green, lineWidth: 3)
            .frame(width: blockWidth, height: 26)
            .position(x: t.anchor.x + blockWidth/2, y: t.anchor.y + 13)
            .allowsHitTesting(false)
    }

    private func gridBackground(size: CGSize) -> some View {
        Canvas { context, canvasSize in
            let step: CGFloat = 24
            var path = Path()
            var x: CGFloat = 0
            while x < canvasSize.width { path.move(to: CGPoint(x: x, y: 0)); path.addLine(to: CGPoint(x: x, y: canvasSize.height)); x += step }
            var y: CGFloat = 0
            while y < canvasSize.height { path.move(to: CGPoint(x: 0, y: y)); path.addLine(to: CGPoint(x: canvasSize.width, y: y)); y += step }
            context.stroke(path, with: .color(.gray.opacity(0.08)), lineWidth: 0.5)
        }
        .frame(width: max(size.width, contentExtent.width), height: max(size.height, contentExtent.height))
        .allowsHitTesting(false)
    }
}

// MARK: - Palette drop transfer

struct PaletteDrop: Codable, Transferable {
    let kind: BlockKind

    static var transferRepresentation: some TransferRepresentation {
        CodableRepresentation(contentType: .paletteDrop)
    }
}

extension UTType {
    static let paletteDrop = UTType(exportedAs: "cc.saillant.zacus.hub.palette-drop")
}

// MARK: - Tiny undo broker shared with editor view

final class UndoBroker {
    static let shared = UndoBroker()
    var onSnapshot: ((BlocksDocument) -> Void)?
    func push(_ doc: BlocksDocument) { onSnapshot?(doc) }
}
