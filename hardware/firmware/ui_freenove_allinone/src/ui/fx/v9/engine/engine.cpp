#include "ui/fx/v9/engine/engine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>

#include "ui/fx/v9/engine/timeline_load.h"
#include "ui/fx/v9/effects/plasma.h"
#include "ui/fx/v9/effects/rasterbars.h"
#include "ui/fx/v9/effects/rotozoom.h"
#include "ui/fx/v9/effects/scrolltext.h"
#include "ui/fx/v9/effects/shadebobs.h"
#include "ui/fx/v9/effects/starfield.h"
#include "ui/fx/v9/effects/transition_flash.h"
#include "ui/fx/v9/effects/tunnel3d.h"
#include "ui/fx/v9/effects/wirecube.h"
#include "ui/fx/v9/effects/hourglass.h"

namespace fx {

namespace {

ModType parseModType(const std::string& type_name) {
  if (type_name == "ramp") {
    return ModType::RAMP;
  }
  if (type_name == "ease") {
    return ModType::EASE;
  }
  if (type_name == "beat_pulse") {
    return ModType::BEAT_PULSE;
  }
  if (type_name == "random_hold") {
    return ModType::RANDOM_HOLD;
  }
  if (type_name == "toggle_on_bar") {
    return ModType::TOGGLE_ON_BAR;
  }
  return ModType::SINE;
}

void seedModState(Mod* mod) {
  if (mod == nullptr) {
    return;
  }
  const std::string seed_key = mod->clipId + "|" + mod->param;
  uint32_t seed = static_cast<uint32_t>(std::hash<std::string>{}(seed_key));
  if (seed == 0U) {
    seed = 0x12345678U;
  }
  mod->st.rng = seed;
  mod->st.held = mod->base;
  mod->st.lastBeat = -1;
  mod->st.lastBar = -1;
  mod->st.toggle = false;
}

void configureModFromArgs(Mod* mod, const Modulation& src) {
  if (mod == nullptr) {
    return;
  }
  mod->type = parseModType(src.type);
  mod->base = paramFloat(src.args, "base", mod->base);
  mod->amp = paramFloat(src.args, "amp", mod->amp);
  mod->freqHz = paramFloat(src.args, "freqHz", paramFloat(src.args, "freq", mod->freqHz));
  mod->phase = paramFloat(src.args, "phase", mod->phase);
  mod->t0 = paramFloat(src.args, "t0", mod->t0);
  mod->t1 = paramFloat(src.args, "t1", mod->t1);
  mod->v0 = paramFloat(src.args, "v0", mod->v0);
  mod->v1 = paramFloat(src.args, "v1", mod->v1);
  mod->amount = paramFloat(src.args, "amount", mod->amount);
  mod->decay = paramFloat(src.args, "decay", mod->decay);
  mod->holdBeats = paramInt(src.args, "holdBeats", paramInt(src.args, "hold_beats", mod->holdBeats));
  mod->minV = paramFloat(src.args, "min", mod->minV);
  mod->maxV = paramFloat(src.args, "max", mod->maxV);
  mod->a = paramFloat(src.args, "a", mod->a);
  mod->b = paramFloat(src.args, "b", mod->b);
  if (mod->holdBeats <= 0) {
    mod->holdBeats = 1;
  }
  if (mod->t1 < mod->t0) {
    const float tmp = mod->t0;
    mod->t0 = mod->t1;
    mod->t1 = tmp;
  }
  seedModState(mod);
}

void seedNumericParamDefaults(ClipInstance* clip) {
  if (clip == nullptr) {
    return;
  }
  clip->params.f.clear();
  for (const auto& kv : clip->clip.params) {
    const std::string& raw = kv.second;
    if (raw.empty()) {
      continue;
    }
    char* end = nullptr;
    const float value = std::strtof(raw.c_str(), &end);
    if (end != raw.c_str() && end != nullptr && *end == '\0') {
      clip->params.f[kv.first] = value;
    }
  }
}

bool findParamValue(const ParamTable& params, const char* key, float* out_value) {
  if (key == nullptr || out_value == nullptr) {
    return false;
  }
  std::unordered_map<std::string, float>::const_iterator it = params.f.find(key);
  if (it == params.f.end()) {
    return false;
  }
  *out_value = it->second;
  return true;
}

void applyStaticClipParams(ClipInstance* clip) {
  if (clip == nullptr || clip->fx == nullptr) {
    return;
  }

  const std::string& fx_name = clip->clip.fx;

  if (fx_name == "plasma") {
    effects::PlasmaFx* plasma = static_cast<effects::PlasmaFx*>(clip->fx.get());
    plasma->speed = paramFloat(clip->clip.params, "speed", plasma->speed);
    plasma->contrast = paramFloat(clip->clip.params, "contrast", plasma->contrast);
    return;
  }

  if (fx_name == "rasterbars") {
    effects::RasterbarsFx* bars = static_cast<effects::RasterbarsFx*>(clip->fx.get());
    bars->bars = paramInt(clip->clip.params, "bars", bars->bars);
    bars->thickness = paramInt(clip->clip.params, "thickness", bars->thickness);
    bars->amp = paramFloat(clip->clip.params, "amp", bars->amp);
    bars->speed = paramFloat(clip->clip.params, "speed", bars->speed);
    bars->gradientSteps = paramInt(clip->clip.params, "gradientSteps", bars->gradientSteps);
    return;
  }

  if (fx_name == "starfield") {
    effects::StarfieldFx* stars = static_cast<effects::StarfieldFx*>(clip->fx.get());
    stars->layers = paramInt(clip->clip.params, "layers", stars->layers);
    stars->stars = paramInt(clip->clip.params, "stars", stars->stars);
    stars->speedNear = paramFloat(clip->clip.params, "speedNear", stars->speedNear);
    stars->driftAmp = paramFloat(clip->clip.params, "driftAmp", stars->driftAmp);
    return;
  }

  if (fx_name == "shadebobs") {
    effects::ShadebobsFx* bobs = static_cast<effects::ShadebobsFx*>(clip->fx.get());
    bobs->bobs = paramInt(clip->clip.params, "bobs", bobs->bobs);
    bobs->radius = paramInt(clip->clip.params, "radius", bobs->radius);
    bobs->decay = paramFloat(clip->clip.params, "decay", bobs->decay);
    bobs->invertOnBar = paramBool(clip->clip.params, "invertOnBar", bobs->invertOnBar);
    return;
  }

  if (fx_name == "scrolltext") {
    effects::ScrolltextFx* scroll = static_cast<effects::ScrolltextFx*>(clip->fx.get());
    scroll->textId = paramStr(clip->clip.params, "textId", scroll->textId);
    scroll->speed = paramFloat(clip->clip.params, "speed", scroll->speed);
    scroll->waveAmp = paramInt(clip->clip.params, "waveAmp", scroll->waveAmp);
    scroll->wavePeriod = paramInt(clip->clip.params, "wavePeriod", scroll->wavePeriod);
    scroll->y = paramInt(clip->clip.params, "y", scroll->y);
    scroll->shadow = paramBool(clip->clip.params, "shadow", scroll->shadow);
    scroll->highlight = paramBool(clip->clip.params, "highlight", scroll->highlight);
    return;
  }

  if (fx_name == "transition_flash") {
    effects::TransitionFlashFx* flash = static_cast<effects::TransitionFlashFx*>(clip->fx.get());
    flash->flashFrames = paramInt(clip->clip.params, "flashFrames", flash->flashFrames);
    flash->fadeOut = paramFloat(clip->clip.params, "fadeOut", flash->fadeOut);
    return;
  }

  if (fx_name == "tunnel3d") {
    effects::Tunnel3DFx* tunnel = static_cast<effects::Tunnel3DFx*>(clip->fx.get());
    tunnel->speed = paramFloat(clip->clip.params, "speed", tunnel->speed);
    tunnel->rotSpeed = paramFloat(clip->clip.params, "rotSpeed", tunnel->rotSpeed);
    tunnel->beatKick = static_cast<uint8_t>(paramInt(clip->clip.params, "beatKick", tunnel->beatKick));
    tunnel->palSpeed = static_cast<uint8_t>(paramInt(clip->clip.params, "palSpeed", tunnel->palSpeed));
    return;
  }

  if (fx_name == "rotozoom") {
    effects::RotozoomFx* roto = static_cast<effects::RotozoomFx*>(clip->fx.get());
    roto->rotSpeed = paramFloat(clip->clip.params, "rotSpeed", roto->rotSpeed);
    roto->zoomBase = paramFloat(clip->clip.params, "zoomBase", roto->zoomBase);
    roto->zoomAmp = paramFloat(clip->clip.params, "zoomAmp", roto->zoomAmp);
    roto->zoomFreq = paramFloat(clip->clip.params, "zoomFreq", roto->zoomFreq);
    roto->scrollU = paramFloat(clip->clip.params, "scrollU", roto->scrollU);
    roto->scrollV = paramFloat(clip->clip.params, "scrollV", roto->scrollV);
    roto->beatKick = static_cast<uint8_t>(paramInt(clip->clip.params, "beatKick", roto->beatKick));
    roto->palSpeed = static_cast<uint8_t>(paramInt(clip->clip.params, "palSpeed", roto->palSpeed));
    return;
  }

  if (fx_name == "wirecube") {
    effects::WireCubeFx* cube = static_cast<effects::WireCubeFx*>(clip->fx.get());
    cube->rotX = paramFloat(clip->clip.params, "rotX", cube->rotX);
    cube->rotY = paramFloat(clip->clip.params, "rotY", cube->rotY);
    cube->rotZ = paramFloat(clip->clip.params, "rotZ", cube->rotZ);
    cube->zOffset = paramFloat(clip->clip.params, "zOffset", cube->zOffset);
    cube->fov = paramFloat(clip->clip.params, "fov", cube->fov);
    cube->intensity = static_cast<uint8_t>(paramInt(clip->clip.params, "intensity", cube->intensity));
    cube->beatPulse = paramBool(clip->clip.params, "beatPulse", cube->beatPulse);
    return;
  }

  if (fx_name == "hourglass") {
    effects::HourglassFx* hg = static_cast<effects::HourglassFx*>(clip->fx.get());
    hg->speed = paramFloat(clip->clip.params, "speed", hg->speed);
    hg->glitch = paramFloat(clip->clip.params, "glitch", hg->glitch);
  }
}

void applyModulatedParams(ClipInstance* clip) {
  if (clip == nullptr || clip->fx == nullptr) {
    return;
  }

  const std::string& fx_name = clip->clip.fx;
  float value = 0.0f;
  if (fx_name == "plasma") {
    effects::PlasmaFx* plasma = static_cast<effects::PlasmaFx*>(clip->fx.get());
    if (findParamValue(clip->params, "speed", &value)) {
      plasma->speed = value;
    }
    if (findParamValue(clip->params, "contrast", &value)) {
      plasma->contrast = value;
    }
    return;
  }

  if (fx_name == "rasterbars") {
    effects::RasterbarsFx* bars = static_cast<effects::RasterbarsFx*>(clip->fx.get());
    if (findParamValue(clip->params, "amp", &value)) {
      bars->amp = value;
    }
    if (findParamValue(clip->params, "speed", &value)) {
      bars->speed = value;
    }
    return;
  }

  if (fx_name == "starfield") {
    effects::StarfieldFx* stars = static_cast<effects::StarfieldFx*>(clip->fx.get());
    if (findParamValue(clip->params, "speedNear", &value)) {
      stars->speedNear = value;
    }
    if (findParamValue(clip->params, "driftAmp", &value)) {
      stars->driftAmp = value;
    }
    return;
  }

  if (fx_name == "scrolltext") {
    effects::ScrolltextFx* scroll = static_cast<effects::ScrolltextFx*>(clip->fx.get());
    if (findParamValue(clip->params, "speed", &value)) {
      scroll->speed = value;
    }
    if (findParamValue(clip->params, "waveAmp", &value)) {
      scroll->waveAmp = static_cast<int>(value);
    }
    return;
  }

  if (fx_name == "tunnel3d") {
    effects::Tunnel3DFx* tunnel = static_cast<effects::Tunnel3DFx*>(clip->fx.get());
    if (findParamValue(clip->params, "speed", &value)) {
      tunnel->speed = value;
    }
    if (findParamValue(clip->params, "rotSpeed", &value)) {
      tunnel->rotSpeed = value;
    }
    return;
  }

  if (fx_name == "rotozoom") {
    effects::RotozoomFx* roto = static_cast<effects::RotozoomFx*>(clip->fx.get());
    if (findParamValue(clip->params, "rotSpeed", &value)) {
      roto->rotSpeed = value;
    }
    if (findParamValue(clip->params, "zoomAmp", &value)) {
      roto->zoomAmp = value;
    }
    if (findParamValue(clip->params, "zoomBase", &value)) {
      roto->zoomBase = value;
    }
    return;
  }

  if (fx_name == "wirecube") {
    effects::WireCubeFx* cube = static_cast<effects::WireCubeFx*>(clip->fx.get());
    if (findParamValue(clip->params, "rotX", &value)) {
      cube->rotX = value;
    }
    if (findParamValue(clip->params, "rotY", &value)) {
      cube->rotY = value;
    }
    if (findParamValue(clip->params, "rotZ", &value)) {
      cube->rotZ = value;
    }
    return;
  }

  if (fx_name == "hourglass") {
    effects::HourglassFx* hg = static_cast<effects::HourglassFx*>(clip->fx.get());
    if (findParamValue(clip->params, "speed", &value)) {
      hg->speed = value;
    }
    if (findParamValue(clip->params, "glitch", &value)) {
      hg->glitch = value;
    }
  }
}

}  // namespace

Engine::Engine()
{
  luts.init();
}

void Engine::registerFx(const std::string& name, FxFactory factory)
{
  factories[name] = std::move(factory);
}

bool Engine::loadTimeline(const Timeline& tl)
{
  metaInfo = tl.meta;

  clips.clear();
  clips.reserve(tl.clips.size());
  for (const Clip& c : tl.clips) {
    ClipInstance ci;
    ci.clip = c;
    ci.track = parseTrack(c.track);

    auto it = factories.find(c.fx);
    if (it == factories.end()) {
      // Unknown FX name: skip (or handle as error in your app)
      continue;
    }
    ci.fx = it->second();

    seedNumericParamDefaults(&ci);

    // Attach mods for this clip
    ci.mods.clear();
    for (const Modulation& m : tl.mods) {
      if (m.clip == c.id) {
        Mod mod;
        mod.clipId = m.clip;
        mod.param  = m.param;
        configureModFromArgs(&mod, m);
        ci.mods.push_back(std::move(mod));
      }
    }
    applyStaticClipParams(&ci);

    clips.push_back(std::move(ci));
  }

  ensureBuffers();
  return true;
}

void Engine::init()
{
  ctx = {};
  ctx.frame = 0;
  ctx.demoTime = 0.0f;
  ctx.bpm = metaInfo.bpm;
  ctx.seed = metaInfo.seed;
  ctx.internalW = metaInfo.internal.w;
  ctx.internalH = metaInfo.internal.h;
  ctx.internalFmt = metaInfo.internal.fmt;

  rng.seed(ctx.seed);

  for (ClipInstance& c : clips) {
    c.initialized = false;
    seedNumericParamDefaults(&c);
    applyStaticClipParams(&c);
  }
}

void Engine::computeBeatBar(float dt)
{
  // beats per second
  const float bps = ctx.bpm / 60.0f;
  const float beatDur = (bps > 0.0f) ? (1.0f / bps) : 0.5f;

  float prevBeatTime = ctx.demoTime;
  ctx.demoTime += dt;

  uint32_t prevBeat = (uint32_t)floorf(prevBeatTime / beatDur);
  uint32_t newBeat  = (uint32_t)floorf(ctx.demoTime / beatDur);

  ctx.beatHit = (newBeat != prevBeat);
  ctx.beat = newBeat;

  ctx.beatPhase = (beatDur > 0.0f) ? fmodf(ctx.demoTime, beatDur) / beatDur : 0.0f;

  uint32_t prevBar = prevBeat / 4;
  uint32_t newBar  = newBeat / 4;
  ctx.barHit = (newBar != prevBar);
  ctx.bar = newBar;
}

void Engine::tick(float dtSeconds)
{
  ctx.dt = dtSeconds;
  computeBeatBar(dtSeconds);

  // Update all active clips at this time
  for (ClipInstance& ci : clips) {
    if (ctx.demoTime < ci.clip.t0 || ctx.demoTime >= ci.clip.t1) continue;

    ctx.t = ctx.demoTime - ci.clip.t0;

    // Clip seed: global ^ clip seed ^ hash(id)
    uint32_t clipSeed = metaInfo.seed ^ ci.clip.seed;
    ctx.seed = clipSeed;

    if (!ci.initialized) {
      ci.fx->init(ctx);
      ci.initialized = true;
    }

    applyMods(ci.mods, ci.params, ctx.t, ctx.dt, ctx.beat, ctx.bar, ctx.beatPhase, ctx.beatHit, ctx.barHit);
    applyModulatedParams(&ci);

    ci.fx->update(ctx);
  }

  ctx.frame++;
}

void Engine::ensureBuffers()
{
  int w = metaInfo.internal.w;
  int h = metaInfo.internal.h;
  size_t sz = (size_t)w * (size_t)h;

  trackBG.assign(sz, 0);
  trackMID.assign(sz, 0);
  trackUI.assign(sz, 0);
}

RenderTarget Engine::makeTrackTarget(std::vector<uint8_t>& buf)
{
  RenderTarget rt{};
  rt.pixels = buf.data();
  rt.w = metaInfo.internal.w;
  rt.h = metaInfo.internal.h;
  rt.strideBytes = metaInfo.internal.w;
  rt.fmt = PixelFormat::I8;
  rt.palette565 = internalRt.palette565;
  rt.aligned16 = (((uintptr_t)rt.pixels & 15u) == 0u) && ((rt.strideBytes & 15u) == 0u);
  return rt;
}

void Engine::renderTrack(Track tr, RenderTarget& dst)
{
  // Render all active clips of this track into dst (I8)
  for (ClipInstance& ci : clips) {
    if (ci.track != tr) continue;
    if (ctx.demoTime < ci.clip.t0 || ctx.demoTime >= ci.clip.t1) continue;

    FxContext local = ctx;
    local.t = local.demoTime - ci.clip.t0;
    local.seed = metaInfo.seed ^ ci.clip.seed;

    // FX renders into dst directly (BG), or into a temp and blend (for now: direct REPLACE).
    ci.fx->render(local, dst);
  }
}

void Engine::render()
{
  render(internalRt, outputRt);
}

void Engine::render(RenderTarget& internal, RenderTarget& output)
{
  // internal is low-res I8 (recommended). output is RGB565.
  if (metaInfo.internal.fmt != PixelFormat::I8) return;

  RenderTarget bg = makeTrackTarget(trackBG);
  RenderTarget mid = makeTrackTarget(trackMID);
  RenderTarget ui = makeTrackTarget(trackUI);

  gfx::fill_i8(bg, 0);
  gfx::fill_i8(mid, 0);
  gfx::fill_i8(ui, 0);

  renderTrack(Track::BG, bg);
  renderTrack(Track::MID, mid);
  renderTrack(Track::UI, ui);

  // Composite tracks in order BG -> MID -> UI (I8)
  RenderTarget comp = bg;
  gfx::blend_i8(comp, mid, BlendMode::ADD_CLAMP);
  gfx::blend_i8(comp, ui, BlendMode::ADD_CLAMP);

  // Upscale to output RGB565
  gfx::upscale_nearest_i8_to_rgb565(comp, output);
}

} // namespace fx
