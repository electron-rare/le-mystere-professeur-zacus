<script lang="ts">
  import Button from '$lib/components/ui/Button.svelte'
  import Panel from '$lib/components/ui/Panel.svelte'
  import SectionHeader from '$lib/components/ui/SectionHeader.svelte'
  import { opsStore } from '$lib/stores/ops.store'
  import { runtimeStore } from '$lib/stores/runtime.store'

  let eventFilter: 'all' | 'status' | 'step_change' | 'error' = 'all'

  $: state = $runtimeStore
  $: streamEvents =
    eventFilter === 'all' ? state.streamEvents : state.streamEvents.filter((entry) => entry.raw.type === eventFilter)
</script>

<section class="deck-shell">
  <Panel>
    <SectionHeader title="Diagnostics avancés" subtitle="Audit, stream, payload brut">
      <svelte:fragment slot="actions">
        <Button on:click={() => opsStore.refreshAudit()}>Rafraîchir trace</Button>
      </svelte:fragment>
    </SectionHeader>

    <div style="margin-top:0.7rem;display:flex;gap:0.45rem;align-items:center;flex-wrap:wrap;">
      <span class="badge badge-info">Stream {state.streamStatus}</span>
      <select class="select" style="max-width:220px;" bind:value={eventFilter}>
        <option value="all">Tous</option>
        <option value="status">Status</option>
        <option value="step_change">Transitions</option>
        <option value="error">Erreurs</option>
      </select>
    </div>

    <div class="split" style="margin-top:0.75rem;">
      <div>
        <p style="margin:0 0 0.35rem 0;color:var(--ink-500);font-size:0.8rem;">Audit API</p>
        <div class="code" style="max-height:310px;">
          {#if state.auditEvents.length === 0}
            Aucun événement.
          {:else}
            {#each state.auditEvents as event}
              <pre style="margin:0 0 0.5rem 0;">{JSON.stringify(event, null, 2)}</pre>
            {/each}
          {/if}
        </div>
      </div>
      <div>
        <p style="margin:0 0 0.35rem 0;color:var(--ink-500);font-size:0.8rem;">Stream</p>
        <div class="code" style="max-height:310px;">
          {#if streamEvents.length === 0}
            Aucun message.
          {:else}
            {#each streamEvents as event}
              <div style="margin-bottom:0.5rem;border-bottom:1px dashed var(--line);padding-bottom:0.35rem;">
                <strong>{event.at}</strong>
                <div>{event.raw.type}</div>
              </div>
            {/each}
          {/if}
        </div>
      </div>
    </div>
  </Panel>

  <Panel>
    <SectionHeader title="Payload status brut" subtitle="Référence de mapping runtime" />
    <div style="margin-top:0.75rem;" class="code"><pre style="margin:0;">{JSON.stringify(state.snapshot?.rawStatus || {}, null, 2)}</pre></div>
  </Panel>
</section>
