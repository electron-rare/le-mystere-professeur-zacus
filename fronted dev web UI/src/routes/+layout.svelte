<script lang="ts">
  import { onDestroy, onMount } from 'svelte'
  import { page } from '$app/stores'
  import '../app.css'
  import Badge from '$lib/components/ui/Badge.svelte'
  import Panel from '$lib/components/ui/Panel.svelte'
  import { runtimeStore } from '$lib/stores/runtime.store'
  import { resolveMediaActivation } from '$lib/utils/media-activation'

  const tabs = [
    { href: '/dashboard', label: 'Dashboard' },
    { href: '/scenario', label: 'Scénario' },
    { href: '/designer', label: 'Designer' },
    { href: '/media', label: 'Media Manager' },
    { href: '/network', label: 'Réseau' },
    { href: '/ops', label: 'Diagnostics' },
  ]

  $: state = $runtimeStore
  $: activation = resolveMediaActivation(state.snapshot)
  $: apiLabel = state.apiFlavor === 'story_v2' ? 'API Story V2' : state.apiFlavor === 'freenove_legacy' ? 'API Legacy' : 'API Inconnue'

  onMount(() => {
    void runtimeStore.bootstrap()
    runtimeStore.startPolling()
    void runtimeStore.connectRuntimeStream()
  })

  onDestroy(() => {
    runtimeStore.stopPolling()
    runtimeStore.disconnectRuntimeStream()
  })
</script>

<main class="deck-shell">
  <Panel>
    <div style="display:flex;justify-content:space-between;gap:0.75rem;align-items:flex-start;flex-wrap:wrap;">
      <div>
        <h1 style="margin:0;font-size:1.35rem;">Zacus Mission Deck</h1>
        <p style="margin:0.2rem 0 0 0;color:var(--ink-500);font-size:0.85rem;">{state.base}</p>
      </div>
      <div style="display:flex;gap:0.4rem;flex-wrap:wrap;">
        <Badge tone="info">{apiLabel}</Badge>
        <Badge tone={state.loading ? 'warn' : 'ok'}>{state.loading ? 'Connexion' : 'Connecté'}</Badge>
        <Badge tone="neutral">Flux {state.streamStatus}</Badge>
        {#if state.snapshot?.status}
          <Badge tone="ok">{state.snapshot.status}</Badge>
        {/if}
        {#if activation.active}
          <Badge tone="info">Media Hub actif</Badge>
        {/if}
      </div>
    </div>

    <div class="grid-2" style="margin-top:0.75rem;">
      <div class="kpi">
        <small>Scénario actif</small>
        <strong>{state.snapshot?.scenarioId || '—'}</strong>
      </div>
      <div class="kpi">
        <small>Step</small>
        <strong>{state.snapshot?.currentStep || '—'}</strong>
      </div>
      <div class="kpi">
        <small>Scene</small>
        <strong>{state.snapshot?.currentScreen || '—'}</strong>
      </div>
      <div class="kpi">
        <small>Progression</small>
        <div class="progress"><span style={`width:${Math.max(0, Math.min(100, Math.round(state.snapshot?.progressPct || 0)))}%;`}></span></div>
      </div>
    </div>
  </Panel>

  <nav class="deck-nav" aria-label="Navigation">
    {#each tabs as tab}
      <a href={tab.href} data-sveltekit-preload-data="hover" class={`btn ${$page.url.pathname.startsWith(tab.href) ? 'btn-primary' : 'btn-outline'}`}>{tab.label}</a>
    {/each}
  </nav>

  <slot />
</main>
