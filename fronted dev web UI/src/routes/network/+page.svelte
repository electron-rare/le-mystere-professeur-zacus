<script lang="ts">
  import Button from '$lib/components/ui/Button.svelte'
  import InlineNotice from '$lib/components/ui/InlineNotice.svelte'
  import Panel from '$lib/components/ui/Panel.svelte'
  import SectionHeader from '$lib/components/ui/SectionHeader.svelte'
  import { networkStore } from '$lib/stores/network.store'
  import { runtimeStore } from '$lib/stores/runtime.store'

  $: state = $runtimeStore
</script>

<section class="deck-shell">
  <Panel>
    <SectionHeader title="Réseau" subtitle="Wi-Fi, ESP-NOW, firmware infos">
      <svelte:fragment slot="actions">
        <div style="display:flex;gap:0.35rem;flex-wrap:wrap;">
          <Button on:click={() => networkStore.reconnectWifi()} disabled={Boolean(state.busy.wifiReconnect)}>Reconnect</Button>
          <Button on:click={() => networkStore.setEspNow(false)} disabled={Boolean(state.busy.espNowOff)}>ESPNOW OFF</Button>
          <Button on:click={() => networkStore.setEspNow(true)} disabled={Boolean(state.busy.espNowOn)}>ESPNOW ON</Button>
        </div>
      </svelte:fragment>
    </SectionHeader>

    <div class="split" style="margin-top:0.8rem;">
      <div>
        <p style="margin:0 0 0.35rem 0;color:var(--ink-500);font-size:0.8rem;">Wi-Fi</p>
        <div class="code"><pre style="margin:0;">{JSON.stringify(state.network?.wifi || {}, null, 2)}</pre></div>
      </div>
      <div>
        <p style="margin:0 0 0.35rem 0;color:var(--ink-500);font-size:0.8rem;">ESP-NOW</p>
        <div class="code"><pre style="margin:0;">{JSON.stringify(state.network?.espnow || {}, null, 2)}</pre></div>
      </div>
    </div>

    {#if state.globalError}
      <div style="margin-top:0.75rem;"><InlineNotice tone="error">{state.globalError}</InlineNotice></div>
    {/if}
  </Panel>

  <Panel>
    <SectionHeader title="Firmware" subtitle="Version et capacités détectées" />
    <div style="margin-top:0.75rem;" class="code"><pre style="margin:0;">{JSON.stringify(state.firmwareInfo || {}, null, 2)}</pre></div>
  </Panel>
</section>
