# LVGL Graphics Stack (Freenove ESP32-S3, 320x480)

## Overview
The Freenove UI runtime now supports a deterministic rendering pipeline with explicit fallbacks:

- Default: RGB332 (`LV_COLOR_DEPTH=8`) + RGB565 transfer to TFT SPI.
- Draw buffers: double line buffers (`buf1 + buf2`) with PSRAM-first policy.
- Flush: asynchronous DMA when available.
- Fallback: synchronous flush when DMA path cannot be used.
- Display HAL: single backend entry point (`drivers/display/display_hal.*`) with SPI mutex.

`UiManager` public API stays unchanged.

## Build Flags (freenove_esp32s3*)
- `UI_COLOR_256=1`: enable RGB332 draw path.
- `UI_COLOR_565=0`: keep disabled unless forcing LVGL 16bpp fallback.
- `UI_FORCE_THEME_256=1`: quantize theme colors when running 16bpp.
- `UI_DRAW_BUF_LINES=40`: target line count for LVGL draw buffers.
- `UI_DRAW_BUF_IN_PSRAM=1`: default draw buffers in PSRAM (`FREENOVE_PSRAM_UI_DRAW_BUFFER` mapped to this master flag).
- `UI_DMA_FLUSH_ASYNC=1`: enable non-blocking DMA flush.
- `UI_DMA_TRANS_BUF_LINES=40`: internal SRAM DMA transfer buffer lines.
- `UI_FULL_FRAME_BENCH=0`: full-frame PSRAM mode (bench only).
- `UI_FX_BACKEND_LGFX=1`: request LovyanGFX overlay backend (deterministic fallback to TFT_eSPI if unavailable).
- `UI_FX_SPRITE_W=160`, `UI_FX_SPRITE_H=120`, `UI_FX_TARGET_FPS=18`: low-res FX overlay budget.

## Runtime Modes

### 1) Default perf path
- Draw buffers allocated in `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`.
- `displayFlushCb` launches `pushImageDMA` and defers `lv_disp_flush_ready`.
- `pollAsyncFlush()` finalizes flush when `dmaBusy()==false`.
- `SpiBusManager` enforces one SPI writer at a time (`startWrite`/`endWrite` lock scope).

### 2) PSRAM line-buffer path
- Draw buffers allocated in PSRAM (`MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`).
- Transfer goes through `dma_trans_buf_` in internal DMA-capable SRAM.

### 3) Sync fallback path
- Triggered when DMA init fails, transfer buffer is unavailable, or async is disabled.
- Flush is done with blocking `pushColors` and immediate `lv_disp_flush_ready`.

### 4) Full-frame bench path
- Enabled only with `UI_FULL_FRAME_BENCH=1`.
- Attempts full-frame LVGL buffer in PSRAM, otherwise falls back to line buffers.

## 8bpp Transfer Conversion
In RGB332 mode:
- LVGL renders in 8bpp (`lv_color_t` index-like RGB332 layout).
- Flush converts to RGB565 using a precomputed 256-entry LUT.
- Conversion is applied only on the flushed area.

## No-blocking rule
- `displayFlushCb` must not block when async DMA path is active.
- `lv_disp_flush_ready` is called only after DMA completes (`pollAsyncFlush`).
- No `delay()` is used in rendering flow.

## Debug Commands
- `UI_GFX_STATUS`: print active graphics mode and flush stats.
- `UI_MEM_STATUS`: print LVGL and heap memory status.
