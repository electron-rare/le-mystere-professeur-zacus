#pragma once
#include "ui/fx/v9/effects/fx_base.h"
#include <cstdint>

namespace fx::effects {

// Simple 3D wireframe cube (vector style) rendered into I8.
// Very cheap: 8 vertices + 12 lines per frame.
class WireCubeFx : public FxBase {
public:
  explicit WireCubeFx(FxServices s);

  void init(const FxContext& ctx) override;
  void update(const FxContext& ctx) override;
  void render(const FxContext& ctx, RenderTarget& rt) override;

  float rotX = 0.21f;   // rotations per second
  float rotY = 0.27f;
  float rotZ = 0.11f;

  float zOffset = 3.0f; // camera distance
  float fov = 1.4f;     // projection strength
  uint8_t intensity = 220;

  bool beatPulse = true;

private:
  float ax_ = 0.0f, ay_ = 0.0f, az_ = 0.0f;
  float pulse_ = 0.0f;

  static void plot_(RenderTarget& rt, int x, int y, uint8_t v);
  static void line_(RenderTarget& rt, int x0, int y0, int x1, int y1, uint8_t v);
};

} // namespace fx::effects
