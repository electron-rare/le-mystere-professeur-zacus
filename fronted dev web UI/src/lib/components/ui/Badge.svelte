<script lang="ts">
  export let tone: 'info' | 'ok' | 'warn' | 'err' | 'neutral' = 'neutral'
</script>

<span class={`badge ${tone === 'neutral' ? '' : `badge-${tone}`}`} {...$$restProps}>
  <slot />
</span>
