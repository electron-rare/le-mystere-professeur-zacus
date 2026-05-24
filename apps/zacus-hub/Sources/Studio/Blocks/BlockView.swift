import SwiftUI

/// Visual representation of a block on the canvas.
struct BlockView: View {
    @Binding var node: BlockNode
    var isSelected: Bool
    var onSelect: () -> Void
    var onDelete: () -> Void

    private var spec: BlockSpec { BlockCatalog.spec(node.kind) }
    private var color: Color { node.kind.category.color }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            header
            ForEach(spec.params, id: \.name) { p in
                paramField(p)
            }
            ForEach(spec.slots, id: \.name) { slot in
                slotMouth(slot)
            }
        }
        .padding(10)
        .frame(width: blockWidth, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .fill(color.opacity(0.92))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .stroke(isSelected ? Color.white : Color.black.opacity(0.18), lineWidth: isSelected ? 2.5 : 1)
        )
        .shadow(color: .black.opacity(isSelected ? 0.25 : 0.12), radius: isSelected ? 8 : 3, x: 0, y: 2)
        .foregroundStyle(.white)
        .contentShape(Rectangle())
        .onTapGesture { onSelect() }
        .contextMenu {
            Button(role: .destructive) { onDelete() } label: { Label("Supprimer", systemImage: "trash") }
        }
    }

    private var header: some View {
        HStack(spacing: 6) {
            Image(systemName: icon).font(.caption.bold())
            Text(spec.title).font(.subheadline.bold()).lineLimit(1)
            Spacer(minLength: 4)
        }
    }

    private var icon: String {
        switch node.kind.category {
        case .scene:    return "rectangle.stack"
        case .npc:      return "person.wave.2"
        case .audio:    return "speaker.wave.2"
        case .lcd:      return "display"
        case .hardware: return "cpu"
        case .espnow:   return "wifi.router"
        case .box3:     return "shippingbox"
        case .m5:       return "rectangle.3.group"
        case .plip:     return "phone.fill"
        case .logic:    return "function"
        }
    }

    @ViewBuilder private func slotMouth(_ slot: BlockSpec.SlotSpec) -> some View {
        let filled = node.slots[slot.name] != nil
        HStack {
            Text(slot.label).font(.caption.bold()).foregroundStyle(.white.opacity(0.9))
            Spacer()
            Image(systemName: filled ? "chevron.down.circle.fill" : "chevron.down.circle.dotted")
                .foregroundStyle(.white.opacity(filled ? 1 : 0.6))
        }
        .padding(.horizontal, 8).padding(.vertical, 6)
        .background(
            RoundedRectangle(cornerRadius: 5)
                .strokeBorder(style: StrokeStyle(lineWidth: 1.5, dash: [4,3]))
                .foregroundStyle(.white.opacity(filled ? 0.0 : 0.65))
        )
        .background(
            RoundedRectangle(cornerRadius: 5)
                .fill(.black.opacity(0.18))
        )
    }

    @ViewBuilder private func paramField(_ p: BlockSpec.ParamSpec) -> some View {
        let binding = Binding<String>(
            get: { node.params[p.name] ?? "" },
            set: { node.params[p.name] = $0 }
        )
        VStack(alignment: .leading, spacing: 2) {
            Text(p.label).font(.caption2).foregroundStyle(.white.opacity(0.85))
            switch p.kind {
            case .multiline:
                TextEditor(text: binding)
                    .font(.caption.monospaced())
                    .frame(minHeight: 44, maxHeight: 70)
                    .scrollContentBackground(.hidden)
                    .background(Color.black.opacity(0.18))
                    .cornerRadius(4)
            case .choice(let opts):
                Picker(p.label, selection: binding) {
                    ForEach(opts, id: \.self) { Text($0).tag($0) }
                }
                .pickerStyle(.menu)
                .labelsHidden()
                .tint(.white)
            default:
                TextField(p.placeholder, text: binding)
                    .textFieldStyle(.plain)
                    .font(.caption.monospaced())
                    .padding(.horizontal, 6).padding(.vertical, 3)
                    .background(Color.black.opacity(0.18))
                    .cornerRadius(4)
            }
        }
    }
}

let blockWidth: CGFloat = 240
let blockMinHeight: CGFloat = 56
let snapDistance: CGFloat = 28

/// Approximate vertical height for layout/snap calc.
func estimatedHeight(of node: BlockNode) -> CGFloat {
    let spec = BlockCatalog.spec(node.kind)
    var h: CGFloat = 38  // header + padding
    for p in spec.params {
        switch p.kind {
        case .multiline: h += 78
        default:         h += 38
        }
    }
    h += CGFloat(spec.slots.count) * 34
    return max(blockMinHeight, h)
}
