<script lang="ts">
  import Badge from '$lib/components/ui/Badge.svelte'
  import Button from '$lib/components/ui/Button.svelte'
  import InlineNotice from '$lib/components/ui/InlineNotice.svelte'
  import Panel from '$lib/components/ui/Panel.svelte'
  import SectionHeader from '$lib/components/ui/SectionHeader.svelte'
  import { runtimeStore } from '$lib/stores/runtime.store'
  import { scenarioStore } from '$lib/stores/scenario.store'
  import { resolveMediaActivation } from '$lib/utils/media-activation'

  let showOrchestrator = false
  let eventFilter: 'all' | 'status' | 'step_change' | 'error' = 'all'

  $: state = $runtimeStore
  $: mediaActivation = resolveMediaActivation(state.snapshot)
  $: streamEvents =
    eventFilter === 'all' ? state.streamEvents : state.streamEvents.filter((entry) => entry.raw.type === eventFilter)

  const run = async (action: () => Promise<void>) => {
    await action()
  }
</script>

<section class="deck-shell">
  <Panel>
    <SectionHeader title="Contrôles scénario" subtitle="Pilotage runtime et quick actions">
      <svelte:fragment slot="actions">
        <Button on:click={() => runtimeStore.refreshSnapshot()} disabled={state.loading}>Actualiser</Button>
      </svelte:fragment>
    </SectionHeader>

    <div class="grid-2" style="margin-top:0.85rem;">
      <div>
        <p style="margin:0 0 0.35rem 0;color:var(--ink-500);font-size:0.8rem;">Démarrage rapide</p>
        <div style="display:grid;gap:0.4rem;max-height:260px;overflow:auto;">
          {#if state.scenarios.length === 0}
            <p style="margin:0;color:var(--ink-500);font-size:0.85rem;">Aucun scénario détecté.</p>
          {/if}
          {#each state.scenarios as scenario}
            <button
              type="button"
              class="list-button"
              style={state.activeScenario === scenario.id ? 'border-color: var(--accent); background: var(--accent-soft);' : ''}
              on:click={async () => {
                await run(() => scenarioStore.start(scenario.id))
                showOrchestrator = true
              }}
            >
              <strong>{scenario.id}</strong>
              <div style="display:flex;justify-content:space-between;gap:0.5rem;color:var(--ink-500);font-size:0.75rem;margin-top:0.2rem;">
                <span>{scenario.description || 'Scénario disponible'}</span>
                <span>{scenario.estimated_duration_s || scenario.duration_s || '—'}s</span>
              </div>
            </button>
          {/each}
        </div>

        <div style="display:flex;flex-wrap:wrap;gap:0.4rem;margin-top:0.7rem;">
          {#if state.capabilities.canPause}
            <Button on:click={() => scenarioStore.pause()} disabled={state.snapshot?.status !== 'running' || state.busy.pause}>Pause</Button>
          {/if}
          {#if state.capabilities.canResume}
            <Button on:click={() => scenarioStore.resume()} disabled={state.snapshot?.status !== 'paused' || state.busy.resume}>Reprise</Button>
          {/if}
          {#if state.capabilities.canSkip}
            <Button on:click={() => scenarioStore.skip()} disabled={Boolean(state.busy.skip)}>Skip</Button>
          {/if}
          {#if state.capabilities.canSkip && !state.capabilities.canSelectScenario && !state.capabilities.canStart}
            <Button on:click={() => scenarioStore.unlock()} disabled={Boolean(state.busy.unlock)}>Unlock</Button>
          {/if}
          <Button variant="primary" on:click={() => (showOrchestrator = true)}>Lancer</Button>
        </div>
      </div>

      <div>
        <p style="margin:0 0 0.35rem 0;color:var(--ink-500);font-size:0.8rem;">Media runtime</p>
        <div class="deck-panel" style="padding:0.7rem;box-shadow:none;">
          <p style="margin:0.1rem 0;">Ready: {state.media?.ready ? 'Oui' : 'Non'}</p>
          <p style="margin:0.1rem 0;">Playing: {state.media?.playing ? 'Oui' : 'Non'}</p>
          <p style="margin:0.1rem 0;">Recording: {state.media?.recording ? 'Oui' : 'Non'}</p>
          <p style="margin:0.1rem 0;">Simulé: {state.media?.record_simulated ? 'Oui' : 'Non'}</p>
          <p style="margin:0.1rem 0;">Erreur: {state.media?.last_error || '—'}</p>
        </div>

        <p style="margin:0.7rem 0 0.35rem 0;color:var(--ink-500);font-size:0.8rem;">Réseau</p>
        <div class="deck-panel" style="padding:0.7rem;box-shadow:none;">
          <p style="margin:0.1rem 0;">WiFi: {String(state.network?.wifi?.ssid || 'inconnu')}</p>
          <p style="margin:0.1rem 0;">Connecté: {state.network?.wifi?.connected ? 'Oui' : 'Non'}</p>
          <p style="margin:0.1rem 0;">ESP-NOW: {state.network?.espnow?.enabled ? 'Actif' : 'Inactif'}</p>
        </div>
      </div>
    </div>

    {#if mediaActivation.active}
      <div style="margin-top:0.75rem;">
        <InlineNotice tone="info">
          Media Hub actif ({mediaActivation.reason}) — priorité screen puis fallback step.
        </InlineNotice>
      </div>
    {/if}

    {#if state.globalError}
      <div style="margin-top:0.75rem;"><InlineNotice tone="error">{state.globalError}</InlineNotice></div>
    {/if}
    {#if state.actionLog}
      <div style="margin-top:0.5rem;"><InlineNotice tone="info">{state.actionLog}</InlineNotice></div>
    {/if}
  </Panel>

  {#if showOrchestrator}
    <Panel>
      <SectionHeader title="Orchestrateur live" subtitle="Flux runtime + filtres" />

      <div style="display:flex;gap:0.4rem;flex-wrap:wrap;margin-top:0.5rem;">
        <Badge tone="info">Flux {state.streamStatus}</Badge>
        <Badge tone="ok">Events {state.streamEvents.length}</Badge>
        <select class="select" style="max-width:200px;" bind:value={eventFilter}>
          <option value="all">Tous</option>
          <option value="status">Status</option>
          <option value="step_change">Transitions</option>
          <option value="error">Erreurs</option>
        </select>
      </div>

      <div class="split" style="margin-top:0.75rem;">
        <div>
          <p style="margin:0 0 0.35rem 0;color:var(--ink-500);font-size:0.8rem;">Events stream</p>
          <div class="code" style="max-height:280px;">
            {#if streamEvents.length === 0}
              Aucun event
            {:else}
              {#each streamEvents as event}
                <div style="margin-bottom:0.5rem;border-bottom:1px dashed var(--line);padding-bottom:0.4rem;">
                  <strong>{event.at}</strong> — {event.raw.type}
                </div>
              {/each}
            {/if}
          </div>
        </div>

        <div>
          <p style="margin:0 0 0.35rem 0;color:var(--ink-500);font-size:0.8rem;">Audit API</p>
          <div class="code" style="max-height:280px;">
            {#if state.auditEvents.length === 0}
              Aucun audit
            {:else}
              {#each state.auditEvents as event}
                <pre style="margin:0 0 0.5rem 0;">{JSON.stringify(event, null, 2)}</pre>
              {/each}
            {/if}
          </div>
        </div>
      </div>
    </Panel>
  {/if}
</section>
