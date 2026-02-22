import dagre from 'dagre'
import type { StoryGraphDocument } from './types'

type LayoutDirection = 'LR' | 'TB'

type LayoutOptions = {
  direction?: LayoutDirection
  nodeWidth?: number
  nodeHeight?: number
}

const DEFAULT_WIDTH = 280
const DEFAULT_HEIGHT = 280

const fallbackLayout = (document: StoryGraphDocument, nodeWidth: number, nodeHeight: number): StoryGraphDocument => {
  const columns = 3
  return {
    ...document,
    nodes: document.nodes.map((node, index) => ({
      ...node,
      x: 48 + (index % columns) * (nodeWidth + 56),
      y: 48 + Math.floor(index / columns) * (nodeHeight + 48),
    })),
  }
}

export const autoLayoutStoryGraph = (
  document: StoryGraphDocument,
  options: LayoutOptions = {},
): StoryGraphDocument => {
  const direction = options.direction ?? 'LR'
  const nodeWidth = options.nodeWidth ?? DEFAULT_WIDTH
  const nodeHeight = options.nodeHeight ?? DEFAULT_HEIGHT

  if (document.nodes.length === 0) {
    return document
  }

  try {
    const graph = new dagre.graphlib.Graph({ directed: true })
    graph.setDefaultEdgeLabel(() => ({}))
    graph.setGraph({
      rankdir: direction,
      ranksep: 150,
      nodesep: 70,
      marginx: 40,
      marginy: 40,
    })

    document.nodes.forEach((node) => {
      graph.setNode(node.id, { width: nodeWidth, height: nodeHeight })
    })
    document.edges.forEach((edge) => {
      graph.setEdge(edge.fromNodeId, edge.toNodeId)
    })

    dagre.layout(graph)

    return {
      ...document,
      nodes: document.nodes.map((node) => {
        const position = graph.node(node.id) as { x: number; y: number } | undefined
        if (!position) {
          return node
        }
        return {
          ...node,
          x: Math.max(8, position.x - nodeWidth / 2),
          y: Math.max(8, position.y - nodeHeight / 2),
        }
      }),
    }
  } catch {
    return fallbackLayout(document, nodeWidth, nodeHeight)
  }
}

