<script lang="ts">
  export let title = ''
  export let subtitle = ''
</script>

<div style="display:flex;justify-content:space-between;align-items:flex-start;gap:0.75rem;">
  <div>
    <h2 style="margin:0;font-size:1rem;">{title}</h2>
    {#if subtitle}
      <p style="margin:0.2rem 0 0 0;color:var(--ink-500);font-size:0.85rem;">{subtitle}</p>
    {/if}
  </div>
  <div>
    <slot name="actions" />
  </div>
</div>
