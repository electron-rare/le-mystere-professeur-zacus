#pragma once
#include "ui/fx/v9/effects/fx_base.h"
#include <vector>
#include <cstdint>

namespace fx::effects {

// 2D rotozoom (pseudo-3D “tunnel-ish” wow effect).
// Uses incremental fixed-point stepping per row (no per-pixel sin/cos).
class RotozoomFx : public FxBase {
public:
  explicit RotozoomFx(FxServices s);

  void init(const FxContext& ctx) override;
  void update(const FxContext& ctx) override;
  void render(const FxContext& ctx, RenderTarget& rt) override;

  float rotSpeed = 0.18f;     // rotations per second
  float zoomBase = 0.90f;     // base zoom
  float zoomAmp  = 0.25f;     // zoom modulation
  float zoomFreq = 0.12f;     // Hz

  float scrollU = 0.25f;      // cycles/sec in texture space
  float scrollV = 0.18f;

  uint8_t beatKick = 18;
  uint8_t palSpeed = 1;

private:
  int w_ = 0;
  int h_ = 0;

  std::vector<uint8_t> tex_; // 256x256 I8
  int32_t uOff_ = 0;         // 16.16
  int32_t vOff_ = 0;         // 16.16
  uint8_t palShift_ = 0;

  void buildTexture_();
};

} // namespace fx::effects
