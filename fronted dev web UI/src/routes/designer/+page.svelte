<script lang="ts">
  import { onMount } from 'svelte'
  import type cytoscape from 'cytoscape'
  import type { Core, ElementDefinition, EventObject } from 'cytoscape'
  import Button from '$lib/components/ui/Button.svelte'
  import InlineNotice from '$lib/components/ui/InlineNotice.svelte'
  import Panel from '$lib/components/ui/Panel.svelte'
  import SectionHeader from '$lib/components/ui/SectionHeader.svelte'
  import type { StoryGraphDocument } from '../../features/story-designer'
  import { designerStore } from '$lib/stores/designer.store'
  import { runtimeStore } from '$lib/stores/runtime.store'

  type CytoscapeFactory = typeof cytoscape

  let container: HTMLDivElement | null = null
  let cy: Core | null = null
  let cyFactory: CytoscapeFactory | null = null
  let syncing = false
  let loadingEngine = true
  let engineError = ''
  let hasInitialFit = false
  let lastSyncedDocument: StoryGraphDocument | null = null
  let lastSelectedNodeId: string | null = null
  let lastSelectedEdgeId: string | null = null

  $: designer = $designerStore
  $: runtime = $runtimeStore
  $: selectedNode = designer.document.nodes.find((node) => node.id === designer.selectedNodeId) ?? null
  $: selectedEdge = designer.document.edges.find((edge) => edge.id === designer.selectedEdgeId) ?? null

  const toElements = (document: StoryGraphDocument): ElementDefinition[] => {
    const nodes: ElementDefinition[] = document.nodes.map((node) => ({
      data: {
        id: node.id,
        label: `${node.stepId}\n${node.screenSceneId}`,
      },
      position: { x: node.x, y: node.y },
      classes: node.isInitial ? 'initial' : '',
    }))

    const edges: ElementDefinition[] = document.edges.map((edge) => ({
      data: {
        id: edge.id,
        source: edge.fromNodeId,
        target: edge.toNodeId,
        label: `${edge.eventType}:${edge.eventName}`,
      },
    }))

    return [...nodes, ...edges]
  }

  const syncCy = (document: StoryGraphDocument, options: { fit?: boolean } = {}) => {
    if (!cy || !designer.ready) {
      return
    }

    const graph = cy
    const shouldFit = options.fit || !hasInitialFit
    const previousZoom = graph.zoom()
    const previousPan = { ...graph.pan() }

    syncing = true
    graph.batch(() => {
      graph.elements().remove()
      graph.add(toElements(document))
    })

    if (shouldFit) {
      graph.fit(undefined, 20)
      hasInitialFit = true
    } else {
      graph.zoom(previousZoom)
      graph.pan(previousPan)
    }

    if (designer.selectedNodeId) {
      graph.$id(designer.selectedNodeId).select()
    }
    if (designer.selectedEdgeId) {
      graph.$id(designer.selectedEdgeId).select()
    }
    lastSelectedNodeId = designer.selectedNodeId
    lastSelectedEdgeId = designer.selectedEdgeId
    syncing = false
  }

  $: if (cy && designer.ready && designer.document !== lastSyncedDocument) {
    syncCy(designer.document, { fit: lastSyncedDocument === null })
    lastSyncedDocument = designer.document
  }

  $: if (
    cy &&
    designer.ready &&
    !syncing &&
    (designer.selectedNodeId !== lastSelectedNodeId || designer.selectedEdgeId !== lastSelectedEdgeId)
  ) {
    const graph = cy
    graph.elements(':selected').unselect()
    if (designer.selectedNodeId) {
      graph.$id(designer.selectedNodeId).select()
    }
    if (designer.selectedEdgeId) {
      graph.$id(designer.selectedEdgeId).select()
    }
    lastSelectedNodeId = designer.selectedNodeId
    lastSelectedEdgeId = designer.selectedEdgeId
  }

  const loadDesignerEngine = async () => {
    loadingEngine = true
    engineError = ''
    try {
      const [cytoscapeImport, { default: dagre }, { default: edgehandles }] = await Promise.all([
        import('cytoscape'),
        import('cytoscape-dagre'),
        import('cytoscape-edgehandles'),
      ])
      const module = cytoscapeImport as unknown as { default?: CytoscapeFactory } & CytoscapeFactory
      const cytoscape = module.default ?? (module as unknown as CytoscapeFactory)
      cytoscape.use(dagre)
      cytoscape.use(edgehandles)
      cyFactory = cytoscape
    } catch (error) {
      engineError = error instanceof Error ? error.message : 'Chargement Cytoscape impossible.'
    } finally {
      loadingEngine = false
    }
  }

  const setupCy = () => {
    if (!container || cy || !cyFactory) {
      return
    }

    const graph = cyFactory({
      container,
      elements: [],
      style: [
        {
          selector: 'node',
          style: {
            'background-color': '#fef3c7',
            label: 'data(label)',
            'text-wrap': 'wrap',
            'text-max-width': '170px',
            'font-size': '10px',
            'border-color': '#d1d5db',
            'border-width': 1,
            width: 190,
            height: 84,
          },
        },
        {
          selector: 'node.initial',
          style: {
            'background-color': '#dcfce7',
            'border-color': '#16a34a',
            'border-width': 2,
          },
        },
        {
          selector: 'edge',
          style: {
            width: 1.6,
            'line-color': '#64748b',
            'target-arrow-color': '#64748b',
            'target-arrow-shape': 'triangle',
            'curve-style': 'bezier',
            label: 'data(label)',
            'font-size': '9px',
            color: '#334155',
            'text-background-opacity': 0.9,
            'text-background-color': '#ffffff',
            'text-background-padding': '2px',
          },
        },
        {
          selector: ':selected',
          style: {
            'border-color': '#f06a28',
            'border-width': 2,
            'line-color': '#f06a28',
            'target-arrow-color': '#f06a28',
          },
        },
      ],
    })

    graph.on('tap', 'node', (event: EventObject) => {
      const id = event.target.id()
      if (designerStore.getState().linkingFromNodeId && designerStore.getState().linkingFromNodeId !== id) {
        designerStore.addEdge(designerStore.getState().linkingFromNodeId as string, id)
        return
      }
      designerStore.selectNode(id)
    })

    graph.on('tap', 'edge', (event: EventObject) => {
      designerStore.selectEdge(event.target.id())
    })

    graph.on('tap', (event: EventObject) => {
      if (event.target === graph) {
        designerStore.selectNode(null)
        designerStore.selectEdge(null)
      }
    })

    graph.on('dragfree', 'node', (event: EventObject) => {
      if (syncing) {
        return
      }
      const node = event.target
      designerStore.updateNode(node.id(), {
        x: Math.round(node.position('x')),
        y: Math.round(node.position('y')),
      })
    })

    cy = graph
    syncCy(designer.document, { fit: true })
    lastSyncedDocument = designer.document
  }

  const doAutoLayout = () => {
    designerStore.autoLayout()
    if (cy) {
      cy.layout({ name: 'dagre', rankDir: 'LR', nodeSep: 50, rankSep: 90, fit: true, padding: 25 } as any).run()
    }
  }

  const isEditableTarget = (target: EventTarget | null) => {
    if (!(target instanceof HTMLElement)) {
      return false
    }
    const tag = target.tagName.toLowerCase()
    return tag === 'input' || tag === 'textarea' || tag === 'select' || target.isContentEditable
  }

  const bindDesignerShortcuts = () => {
    const onKeydown = (event: KeyboardEvent) => {
      const withMeta = event.metaKey || event.ctrlKey
      if (!withMeta || isEditableTarget(event.target)) {
        return
      }

      const key = event.key.toLowerCase()
      if (key === 'z' && event.shiftKey) {
        event.preventDefault()
        designerStore.redo()
        return
      }

      if (key === 'z') {
        event.preventDefault()
        designerStore.undo()
        return
      }

      if (key === 'y') {
        event.preventDefault()
        designerStore.redo()
      }
    }

    window.addEventListener('keydown', onKeydown)
    return () => window.removeEventListener('keydown', onKeydown)
  }

  onMount(() => {
    let destroyed = false
    designerStore.markReady()
    designerStore.validateLocal()
    const unbindShortcuts = bindDesignerShortcuts()

    void (async () => {
      await loadDesignerEngine()
      if (!destroyed && !engineError) {
        setupCy()
      }
    })()

    return () => {
      destroyed = true
      unbindShortcuts()
      if (cy) {
        cy.destroy()
        cy = null
      }
    }
  })
</script>

<section class="deck-shell">
  <Panel>
    <SectionHeader title="Story Designer Scratch-like" subtitle="Cytoscape + YAML roundtrip + validation">
      <svelte:fragment slot="actions">
        <div style="display:flex;gap:0.35rem;flex-wrap:wrap;">
          <Button on:click={() => designerStore.undo()}>Annuler</Button>
          <Button on:click={() => designerStore.redo()}>Rétablir</Button>
          <Button on:click={() => designerStore.validateLocal()}>Valider local</Button>
        </div>
      </svelte:fragment>
    </SectionHeader>

    <div style="display:flex;gap:0.4rem;flex-wrap:wrap;margin-top:0.65rem;">
      <Button on:click={() => designerStore.addNode()} disabled={loadingEngine || Boolean(engineError)}>Ajouter node</Button>
      <Button on:click={doAutoLayout} disabled={loadingEngine || Boolean(engineError)}>Auto-layout</Button>
      <Button on:click={() => selectedNode && designerStore.startLinkMode(selectedNode.id)} disabled={!selectedNode || loadingEngine || Boolean(engineError)}>Mode liaison</Button>
      <Button on:click={() => designerStore.cancelLinkMode()} disabled={!designer.linkingFromNodeId}>Annuler liaison</Button>
      <Button on:click={() => designerStore.importYaml()}>Import YAML → Graphe</Button>
      <Button on:click={() => designerStore.exportYaml()}>Export Graphe → YAML</Button>
      <Button on:click={() => designerStore.runRemoteValidate()} disabled={!runtime.capabilities.canValidate || designer.busyAction !== null}>Valider</Button>
      <Button on:click={() => designerStore.runDeploy()} disabled={!runtime.capabilities.canDeploy || designer.busyAction !== null}>Déployer</Button>
      <Button
        on:click={() => designerStore.runTest()}
        disabled={!runtime.capabilities.canDeploy || !runtime.capabilities.canSelectScenario || !runtime.capabilities.canStart || designer.busyAction !== null}
      >
        Test run
      </Button>
    </div>

    <div style="display:flex;gap:0.35rem;flex-wrap:wrap;margin-top:0.65rem;">
      <span class="badge badge-info">Nodes: {designer.document.nodes.length}</span>
      <span class="badge badge-info">Liens: {designer.document.edges.length}</span>
      {#if designer.linkingFromNodeId}
        <span class="badge badge-warn">Liaison depuis {designer.linkingFromNodeId}</span>
      {/if}
    </div>

    {#if !runtime.capabilities.canValidate || !runtime.capabilities.canDeploy}
      <div style="margin-top:0.65rem;">
        <InlineNotice tone="info">Mode lecture/edition: validate/deploy requierent les APIs Story V2.</InlineNotice>
      </div>
    {/if}
    {#if loadingEngine}
      <div style="margin-top:0.65rem;">
        <InlineNotice tone="info">Initialisation du moteur graphique en cours...</InlineNotice>
      </div>
    {:else if engineError}
      <div style="margin-top:0.65rem;">
        <InlineNotice tone="error">Designer dégradé: {engineError}</InlineNotice>
      </div>
    {/if}

    <div style="margin-top:0.75rem;" class="split">
      <div class="story-canvas-shell">
        <div class="story-canvas" bind:this={container}></div>
        {#if loadingEngine}
          <div class="story-canvas-overlay">Chargement du graphe...</div>
        {:else if engineError}
          <div class="story-canvas-overlay error">Impossible d'initialiser Cytoscape.</div>
        {/if}
      </div>

      <div style="display:grid;gap:0.75rem;">
        <div class="field">
          <label for="scenario-id">Scenario ID</label>
          <input
            id="scenario-id"
            class="input"
            value={designer.document.scenarioId}
            on:input={(event) =>
              designerStore.updateScenarioMeta({
                scenarioId: (event.currentTarget as HTMLInputElement).value,
              })}
          />
        </div>

        {#if selectedNode}
          <div class="deck-panel" style="padding:0.7rem;box-shadow:none;">
            <strong>Node sélectionné</strong>
            <div class="field" style="margin-top:0.45rem;">
              <label for="node-step-id">step_id</label>
              <input id="node-step-id" class="input" value={selectedNode.stepId} on:input={(event) => designerStore.updateNode(selectedNode.id, { stepId: (event.currentTarget as HTMLInputElement).value })} />
            </div>
            <div class="field" style="margin-top:0.45rem;">
              <label for="node-screen-id">screen_scene_id</label>
              <input id="node-screen-id" class="input" value={selectedNode.screenSceneId} on:input={(event) => designerStore.updateNode(selectedNode.id, { screenSceneId: (event.currentTarget as HTMLInputElement).value })} />
            </div>
            <div class="field" style="margin-top:0.45rem;">
              <label for="node-audio-id">audio_pack_id</label>
              <input id="node-audio-id" class="input" value={selectedNode.audioPackId} on:input={(event) => designerStore.updateNode(selectedNode.id, { audioPackId: (event.currentTarget as HTMLInputElement).value })} />
            </div>
            <div style="display:flex;gap:0.4rem;flex-wrap:wrap;margin-top:0.45rem;">
              <Button on:click={() => designerStore.setInitialNode(selectedNode.id)}>Définir initial</Button>
              <Button on:click={() => designerStore.removeNode(selectedNode.id)}>Supprimer node</Button>
            </div>
          </div>
        {/if}

        {#if selectedEdge}
          <div class="deck-panel" style="padding:0.7rem;box-shadow:none;">
            <strong>Lien sélectionné</strong>
            <div class="field" style="margin-top:0.45rem;">
              <label for="edge-event-type">event_type</label>
              <input id="edge-event-type" class="input" value={selectedEdge.eventType} on:input={(event) => designerStore.updateEdge(selectedEdge.id, { eventType: (event.currentTarget as HTMLInputElement).value as any })} />
            </div>
            <div class="field" style="margin-top:0.45rem;">
              <label for="edge-event-name">event_name</label>
              <input id="edge-event-name" class="input" value={selectedEdge.eventName} on:input={(event) => designerStore.updateEdge(selectedEdge.id, { eventName: (event.currentTarget as HTMLInputElement).value })} />
            </div>
            <div class="field" style="margin-top:0.45rem;">
              <label for="edge-priority">priority</label>
              <input id="edge-priority" class="input" type="number" min="0" max="255" value={selectedEdge.priority} on:input={(event) => designerStore.updateEdge(selectedEdge.id, { priority: Number((event.currentTarget as HTMLInputElement).value) || 0 })} />
            </div>
            <Button on:click={() => designerStore.removeEdge(selectedEdge.id)} style="margin-top:0.45rem;">Supprimer lien</Button>
          </div>
        {/if}
      </div>
    </div>

    <div style="margin-top:0.8rem;" class="split">
      <div class="field">
        <label for="designer-yaml">YAML</label>
        <textarea id="designer-yaml" class="textarea" value={designer.draft} on:input={(event) => designerStore.setDraft((event.currentTarget as HTMLTextAreaElement).value)}></textarea>
      </div>
      <div>
        <p style="margin:0 0 0.3rem 0;color:var(--ink-500);font-size:0.8rem;">Statut</p>
        <div class="code">{designer.status}</div>

        {#if designer.importWarnings.length > 0}
          <div style="margin-top:0.5rem;"><InlineNotice tone="info">{designer.importWarnings.join(' · ')}</InlineNotice></div>
        {/if}
        {#if designer.localWarnings.length > 0}
          <div style="margin-top:0.5rem;"><InlineNotice tone="info">{designer.localWarnings.join(' · ')}</InlineNotice></div>
        {/if}
        {#if designer.localErrors.length > 0}
          <div style="margin-top:0.5rem;"><InlineNotice tone="error">{designer.localErrors.join(' · ')}</InlineNotice></div>
        {/if}
      </div>
    </div>
  </Panel>
</section>

<style>
  .story-canvas-shell {
    position: relative;
    min-height: 520px;
  }

  .story-canvas-overlay {
    position: absolute;
    inset: 0;
    border-radius: 0.9rem;
    display: grid;
    place-items: center;
    color: var(--ink-700);
    font-size: 0.9rem;
    background: rgb(255 255 255 / 0.72);
    backdrop-filter: blur(1px);
  }

  .story-canvas-overlay.error {
    color: var(--err);
    background: rgb(255 245 245 / 0.78);
  }
</style>
