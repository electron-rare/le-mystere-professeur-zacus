<script lang="ts">
  import Button from '$lib/components/ui/Button.svelte'
  import InlineNotice from '$lib/components/ui/InlineNotice.svelte'
  import Panel from '$lib/components/ui/Panel.svelte'
  import SectionHeader from '$lib/components/ui/SectionHeader.svelte'
  import { runtimeStore } from '$lib/stores/runtime.store'
  import { scenarioStore } from '$lib/stores/scenario.store'

  let yamlDraft = ''
  let scenarioChoice = ''

  $: state = $runtimeStore
  $: scenarioChoice = state.activeScenario
</script>

<section class="deck-shell">
  <Panel>
    <SectionHeader title="Gestion scénario" subtitle="Sélection, validation, déploiement YAML">
      <svelte:fragment slot="actions">
        <Button on:click={() => (yamlDraft = '')}>Réinitialiser</Button>
      </svelte:fragment>
    </SectionHeader>

    <div class="split" style="margin-top:0.8rem;">
      <div>
        <div class="field">
          <label for="scenario-select">Scénario actif</label>
          <select
            id="scenario-select"
            class="select"
            bind:value={scenarioChoice}
            on:change={(event) => runtimeStore.setActiveScenario((event.currentTarget as HTMLSelectElement).value)}
          >
            <option value="" disabled>Choisir un scénario</option>
            {#each state.scenarios as scenario}
              <option value={scenario.id}>{scenario.id}</option>
            {/each}
          </select>
        </div>

        <div style="display:flex;gap:0.45rem;flex-wrap:wrap;margin-top:0.65rem;">
          <Button
            variant="primary"
            disabled={!state.activeScenario || Boolean(state.busy.start)}
            on:click={() => state.activeScenario && scenarioStore.start(state.activeScenario)}
          >
            Démarrer
          </Button>
          <Button disabled={Boolean(state.busy.validate)} on:click={() => scenarioStore.validateYaml(yamlDraft)}>Valider YAML</Button>
          <Button disabled={Boolean(state.busy.deploy)} on:click={() => scenarioStore.deployYaml(yamlDraft)}>Déployer YAML</Button>
        </div>

        <div style="margin-top:0.85rem;" class="code">
          <pre style="margin:0;">{JSON.stringify(state.snapshot?.story || {}, null, 2)}</pre>
        </div>
      </div>

      <div class="field">
        <label for="yaml-draft">Éditeur YAML rapide</label>
        <textarea id="yaml-draft" class="textarea" bind:value={yamlDraft} placeholder="# colle ton YAML ici"></textarea>
      </div>
    </div>

    {#if state.globalError}
      <div style="margin-top:0.75rem;"><InlineNotice tone="error">{state.globalError}</InlineNotice></div>
    {/if}
    {#if state.actionLog}
      <div style="margin-top:0.5rem;"><InlineNotice tone="info">{state.actionLog}</InlineNotice></div>
    {/if}
  </Panel>
</section>
