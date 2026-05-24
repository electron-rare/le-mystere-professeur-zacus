import SwiftUI

/// Sidebar listing block templates. Drag onto canvas, or click "+" for centre spawn.
struct BlockPaletteView: View {
    var onAdd: (BlockKind) -> Void

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                ForEach(BlockCatalog.byCategory(), id: \.0) { category, specs in
                    section(category, specs: specs)
                }
            }
            .padding(12)
        }
        .frame(width: 240)
        .background(.thinMaterial)
    }

    private func section(_ cat: BlockCategory, specs: [BlockSpec]) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 8) {
                Circle().fill(cat.color).frame(width: 10, height: 10)
                Text(cat.label).font(.subheadline.bold())
            }
            ForEach(specs, id: \.kind) { spec in
                tile(spec, color: cat.color)
            }
        }
    }

    private func tile(_ spec: BlockSpec, color: Color) -> some View {
        HStack {
            Text(spec.title).font(.callout).foregroundStyle(.primary)
            Spacer()
            Button { onAdd(spec.kind) } label: {
                Image(systemName: "plus.circle").foregroundStyle(color)
            }
            .buttonStyle(.plain)
        }
        .padding(.vertical, 6).padding(.horizontal, 8)
        .background(RoundedRectangle(cornerRadius: 6).fill(color.opacity(0.12)))
        .contentShape(Rectangle())
        #if os(macOS)
        .draggable(PaletteDrop(kind: spec.kind)) {
            // drag preview
            HStack(spacing: 6) {
                Circle().fill(color).frame(width: 8, height: 8)
                Text(spec.title).font(.caption.bold())
            }
            .padding(6)
            .background(.thinMaterial)
            .cornerRadius(6)
        }
        #endif
    }
}
