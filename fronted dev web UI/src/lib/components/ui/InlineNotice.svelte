<script lang="ts">
  export let tone: 'info' | 'error' = 'info'
</script>

<div class={`notice ${tone === 'error' ? 'notice-error' : 'notice-info'}`} {...$$restProps}>
  <slot />
</div>
