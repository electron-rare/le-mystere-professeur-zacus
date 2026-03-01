#pragma once
#include "ui/fx/v9/effects/fx_base.h"
#include <vector>
#include <cstdint>

namespace fx::effects {

// Classic texture-mapped tunnel using precomputed polar maps.
// Internal format: I8 (indexed), 256x256 procedural texture.
class Tunnel3DFx : public FxBase {
public:
  explicit Tunnel3DFx(FxServices s);

  void init(const FxContext& ctx) override;
  void update(const FxContext& ctx) override;
  void render(const FxContext& ctx, RenderTarget& rt) override;

  // Speed controls (tuned for 160x120 @ 50fps)
  float speed = 0.80f;     // V scroll cycles per second (0..2)
  float rotSpeed = 0.15f;  // U rotation cycles per second
  uint8_t beatKick = 24;   // palette shift on beat
  uint8_t palSpeed = 1;    // palette shift per frame

private:
  int w_ = 0;
  int h_ = 0;

  std::vector<uint8_t> uMap_;  // angle 0..255 per pixel
  std::vector<uint8_t> vMap_;  // depth 0..255 per pixel
  std::vector<uint8_t> tex_;   // 256*256 texture (I8)

  uint8_t uPhase_ = 0;
  uint8_t vPhase_ = 0;
  uint8_t palShift_ = 0;

  void buildTexture_();
  void buildMaps_(int w, int h);
};

} // namespace fx::effects
