<script lang="ts">
  import { onMount } from 'svelte'
  import Button from '$lib/components/ui/Button.svelte'
  import InlineNotice from '$lib/components/ui/InlineNotice.svelte'
  import Panel from '$lib/components/ui/Panel.svelte'
  import SectionHeader from '$lib/components/ui/SectionHeader.svelte'
  import { mediaStore } from '$lib/stores/media.store'
  import { runtimeStore } from '$lib/stores/runtime.store'
  import { resolveMediaActivation } from '$lib/utils/media-activation'
  import type { MediaKind } from '$lib/runtimeService'

  let recordSeconds = 20
  let recordFile = 'record_01.wav'

  const mediaKinds: MediaKind[] = ['music', 'picture', 'recorder']

  $: mediaState = $mediaStore
  $: runtime = $runtimeStore
  $: mediaActivation = resolveMediaActivation(runtime.snapshot)

  onMount(() => {
    void mediaStore.refreshAll()
  })
</script>

<section class="deck-shell">
  <Panel>
    <SectionHeader title="Media Manager" subtitle="Listing, play/stop, record, debug">
      <svelte:fragment slot="actions">
        <Button on:click={() => mediaStore.refreshAll()}>Rafraîchir fichiers</Button>
      </svelte:fragment>
    </SectionHeader>

    <div style="display:flex;gap:0.4rem;flex-wrap:wrap;margin-top:0.65rem;">
      <span class="badge badge-info">Hub actif: {mediaActivation.active ? 'Oui' : 'Non'}</span>
      <span class="badge badge-info">reason: {mediaActivation.reason}</span>
      <span class="badge badge-ok">playing: {runtime.media?.playing ? 'true' : 'false'}</span>
      <span class="badge badge-ok">recording: {runtime.media?.recording ? 'true' : 'false'}</span>
      <span class="badge badge-warn">record_simulated: {runtime.media?.record_simulated ? 'true' : 'false'}</span>
    </div>

    <div class="grid-2" style="margin-top:0.8rem;">
      {#each mediaKinds as kind}
        <div class="deck-panel" style="padding:0.7rem;box-shadow:none;">
          <div style="display:flex;justify-content:space-between;gap:0.5rem;align-items:center;">
            <strong>{kind}</strong>
            <Button on:click={() => mediaStore.refreshKind(kind)} disabled={mediaState.loading[kind]}>
              {mediaState.loading[kind] ? 'Chargement...' : 'Actualiser'}
            </Button>
          </div>
          <div style="display:grid;gap:0.4rem;margin-top:0.5rem;max-height:220px;overflow:auto;">
            {#if mediaState.files[kind].length === 0}
              <p style="margin:0;color:var(--ink-500);font-size:0.84rem;">Aucun fichier.</p>
            {:else}
              {#each mediaState.files[kind] as item}
                <button class="list-button" type="button" on:click={() => mediaStore.play(item)}>{item}</button>
              {/each}
            {/if}
          </div>
        </div>
      {/each}
    </div>
  </Panel>

  <Panel>
    <SectionHeader title="Commandes média" subtitle="Play/stop et enregistrement">
      <svelte:fragment slot="actions">
        <Button variant="primary" on:click={() => mediaStore.stop()} disabled={Boolean(runtime.busy.mediaStop)}>
          Stop
        </Button>
      </svelte:fragment>
    </SectionHeader>

    <div class="grid-2" style="margin-top:0.8rem;">
      <div class="field">
        <label for="record-seconds">Temps d'enregistrement (s)</label>
        <input id="record-seconds" class="input" type="number" min="1" max="600" bind:value={recordSeconds} />
      </div>
      <div class="field">
        <label for="record-file">Nom du fichier</label>
        <input id="record-file" class="input" bind:value={recordFile} />
      </div>
    </div>

    <div style="display:flex;gap:0.45rem;flex-wrap:wrap;margin-top:0.7rem;">
      <Button
        variant="primary"
        disabled={Boolean(runtime.busy.record)}
        on:click={() => (runtime.media?.recording ? mediaStore.recordStop() : mediaStore.recordStart(recordSeconds, recordFile))}
      >
        {runtime.media?.recording ? 'Stop enregistrement' : 'Démarrer enregistrement'}
      </Button>
      <Button on:click={() => mediaStore.stop()} disabled={!runtime.media?.playing}>Stop lecture</Button>
    </div>

    <div style="margin-top:0.75rem;" class="code">
      <pre style="margin:0;">{JSON.stringify(runtime.media || {}, null, 2)}</pre>
    </div>

    {#if runtime.media?.last_error}
      <div style="margin-top:0.75rem;"><InlineNotice tone="error">media.last_error: {runtime.media.last_error}</InlineNotice></div>
    {/if}
    {#if mediaState.actionError}
      <div style="margin-top:0.5rem;"><InlineNotice tone="error">{mediaState.actionError}</InlineNotice></div>
    {/if}
    {#if runtime.globalError}
      <div style="margin-top:0.5rem;"><InlineNotice tone="error">{runtime.globalError}</InlineNotice></div>
    {/if}
  </Panel>
</section>
