#pragma once
#include "ui/fx/v9/engine/types.h"
#include "ui/fx/v9/engine/timeline.h"
#include "ui/fx/v9/engine/mods.h"
#include "ui/fx/v9/math/rng.h"
#include "ui/fx/v9/math/lut.h"
#include "ui/fx/v9/gfx/blit.h"
#include "ui/fx/v9/assets/assets.h"

#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>

namespace fx {

// Track names
enum class Track : uint8_t { BG, MID, UI };

inline Track parseTrack(const std::string& s) {
  if (s == "BG" || s == "bg") return Track::BG;
  if (s == "MID" || s == "mid") return Track::MID;
  return Track::UI;
}

// Factory: name -> new IFx instance
using FxFactory = std::function<std::unique_ptr<IFx>()>;

// Active clip instance: effect + params + state
struct ClipInstance {
  Clip clip;
  Track track = Track::BG;

  std::unique_ptr<IFx> fx;
  ParamTable params;
  std::vector<Mod> mods;

  bool initialized = false;
};

class Engine {
public:
  Engine();

  void setAssetManager(assets::IAssetManager* am) { assets = am; }
  void registerFx(const std::string& name, FxFactory factory);

  bool loadTimeline(const Timeline& tl);

  // Configure output & internal targets (call after loadTimeline if needed)
  void setInternalTarget(RenderTarget rt) { internalRt = rt; }
  void setOutputTarget(RenderTarget rt) { outputRt = rt; }

  // Init + per-frame
  void init();
  void tick(float dtSeconds); // updates time, beat/bar, mods, calls update()
  void render();              // renders tracks into internalRt then upscales to outputRt

  const FxContext& context() const { return ctx; }
  const TimelineMeta& meta() const { return metaInfo; }

  // For embedding in LVGL: render into given targets directly
  void render(RenderTarget& internal, RenderTarget& output);

private:
  assets::IAssetManager* assets = nullptr;

  TimelineMeta metaInfo{};
  std::vector<ClipInstance> clips;
  std::unordered_map<std::string, FxFactory> factories;

  FxContext ctx{};
  Rng32 rng{};
  SinCosLUT luts{};

  RenderTarget internalRt{};
  RenderTarget outputRt{};

  // Scratch for track compositing (I8)
  std::vector<uint8_t> trackBG;
  std::vector<uint8_t> trackMID;
  std::vector<uint8_t> trackUI;

  void computeBeatBar(float dt);
  void buildClipList();

  void ensureBuffers();
  RenderTarget makeTrackTarget(std::vector<uint8_t>& buf);

  void renderTrack(Track tr, RenderTarget& dst);
};

} // namespace fx
