# LVGL Memory Budget (Freenove ESP32-S3)

## Target Configuration
- Display: `320x480`
- Tick: `33 ms` (`~30 FPS` target)
- Color mode default: `8bpp` (`LV_COLOR_DEPTH=8`)
- `LV_MEM_SIZE`: `UI_LV_MEM_SIZE_KB` (default `160 KB`)

## Buffers

### Draw buffers
Default (`UI_DRAW_BUF_LINES=40`):
- Per buffer size = `320 * 40 * sizeof(lv_color_t)`
- In 8bpp mode: `~12.5 KB` per buffer
- Double-buffer total: `~25 KB`

### Transfer buffer
- `UI_DMA_TRANS_BUF_LINES=40`
- Buffer type: RGB565 transfer (`uint16_t`)
- Size = `320 * 40 * 2` = `~25 KB`

### Full-frame bench (optional)
- 8bpp full frame: `320 * 480 * 1` = `~150 KB`
- 16bpp full frame: `~300 KB`
- Disabled by default (`UI_FULL_FRAME_BENCH=0`).

## Monitoring
Use serial commands:
- `UI_GFX_STATUS`
- `UI_MEM_STATUS`

`UI_MEM_STATUS` includes:
- LVGL heap monitor (`used/free/frag/max_used`) when enabled.
- ESP32 heap snapshots (internal, DMA-capable, PSRAM, largest DMA block).

## Tuning guidance
1. Keep line buffers in internal DMA-capable SRAM for production.
2. Increase `UI_DRAW_BUF_LINES` only when internal free DMA heap is stable.
3. Increase `UI_LV_MEM_SIZE_KB` only if runtime LVGL monitor shows low free headroom.
4. Use PSRAM line buffers only when required, with `dma_trans_buf_` still in internal DMA SRAM.

