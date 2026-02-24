# SCENE_WIN_ETAPE file mapping (phase 1 safe moves)

## Source mapping to modular tree

- `ui_freenove_allinone/src/main.cpp` -> `ui_freenove_allinone/src/app/main.cpp`
- `ui_freenove_allinone/src/scenario_manager.cpp` -> `ui_freenove_allinone/src/app/scenario_manager.cpp`
- `ui_freenove_allinone/src/runtime/app_coordinator.cpp` -> `ui_freenove_allinone/src/app/app_coordinator.cpp`
- `ui_freenove_allinone/src/runtime/serial_command_router.cpp` -> `ui_freenove_allinone/src/app/serial_command_router.cpp`
- `ui_freenove_allinone/src/ui_manager.cpp` -> `ui_freenove_allinone/src/ui/ui_manager.cpp`
- `ui_freenove_allinone/src/ui_fonts.cpp` -> `ui_freenove_allinone/src/ui/ui_fonts.cpp`
- `ui_freenove_allinone/src/audio_manager.cpp` -> `ui_freenove_allinone/src/audio/audio_manager.cpp`
- `ui_freenove_allinone/src/storage_manager.cpp` -> `ui_freenove_allinone/src/storage/storage_manager.cpp`
- `ui_freenove_allinone/src/camera_manager.cpp` -> `ui_freenove_allinone/src/camera/camera_manager.cpp`
- `ui_freenove_allinone/src/button_manager.cpp` -> `ui_freenove_allinone/src/drivers/input/button_manager.cpp`
- `ui_freenove_allinone/src/touch_manager.cpp` -> `ui_freenove_allinone/src/drivers/input/touch_manager.cpp`
- `ui_freenove_allinone/src/hardware_manager.cpp` -> `ui_freenove_allinone/src/drivers/board/hardware_manager.cpp`
- `ui_freenove_allinone/src/network_manager.cpp` -> `ui_freenove_allinone/src/system/network/network_manager.cpp`
- `ui_freenove_allinone/src/media_manager.cpp` -> `ui_freenove_allinone/src/system/media/media_manager.cpp`

## Added phase-1/2 modules

- Display abstraction and SPI arbitration:
  - `ui_freenove_allinone/include/drivers/display/display_hal.h`
  - `ui_freenove_allinone/include/drivers/display/spi_bus_manager.h`
  - `ui_freenove_allinone/src/drivers/display/display_hal_tftespi.cpp`
  - `ui_freenove_allinone/src/drivers/display/display_hal_lgfx.cpp`
  - `ui_freenove_allinone/src/drivers/display/spi_bus_manager.cpp`
- Runtime observability:
  - `ui_freenove_allinone/include/system/boot_report.h`
  - `ui_freenove_allinone/include/system/runtime_metrics.h`
  - `ui_freenove_allinone/include/system/rate_limited_log.h`
  - `ui_freenove_allinone/src/system/boot_report.cpp`
  - `ui_freenove_allinone/src/system/runtime_metrics.cpp`
- Phase-2 interface stubs (owner-task migration prep):
  - `ui_freenove_allinone/include/audio/audio_pipeline.h`
  - `ui_freenove_allinone/include/storage/storage_prefetch.h`
  - `ui_freenove_allinone/include/camera/camera_pipeline.h`
  - `ui_freenove_allinone/include/system/task_topology.h`
  - `ui_freenove_allinone/src/system/task_topology.cpp`
  - `ui_freenove_allinone/include/ui/fx/fx_engine.h`
  - `ui_freenove_allinone/src/ui/fx/fx_engine.cpp`

## Compatibility policy

Legacy root headers are kept as facades (`include/*_manager.h`) and forward to canonical modular headers to avoid breaking external includes during migration.
