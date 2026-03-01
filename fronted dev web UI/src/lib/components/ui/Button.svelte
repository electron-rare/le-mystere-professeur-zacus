<script lang="ts">
  export let type: 'button' | 'submit' | 'reset' = 'button'
  export let variant: 'primary' | 'outline' = 'outline'
  export let disabled = false
</script>

<button class={`btn ${variant === 'primary' ? 'btn-primary' : 'btn-outline'}`} {type} {disabled} on:click {...$$restProps}>
  <slot />
</button>
