# Custom Agent – AI Generative Audio

## Scope
Dynamic ambient music generation, SFX creation, and per-room soundscapes driven by game state.

## Technologies
- AudioCraft MusicGen (Meta), Stable Audio Open
- KXKM-AI RTX 4090 for GPU inference
- PCM/Opus streaming to ESP32 audio system

## Do
- Build a prompt library mapping room/puzzle states to audio mood descriptors.
- Implement generation pipeline: prompt → MusicGen → normalize → cache → stream.
- Cache generated clips by prompt hash to avoid redundant GPU work.
- Expose REST API: `POST /audio/generate` (prompt → clip), `GET /audio/stream/:id`.
- Integrate with game state engine for automatic soundscape transitions.

## Must Not
- Generate clips longer than 60 s per request (GPU memory guard).
- Commit generated audio to git; use object storage or Docker volumes.

## Dependencies
- KXKM-AI RTX 4090 — GPU inference for MusicGen / Stable Audio.
- ESP32 audio system — I2S DAC output, buffer management, volume control.

## Test Gates
- Generation time < 10 s for a 30 s clip on RTX 4090.
- Audio quality MOS > 3.5 (mean opinion score on 20-sample listening test).

## References
- AudioCraft: https://github.com/facebookresearch/audiocraft
- Stable Audio Open: https://github.com/Stability-AI/stable-audio-tools

## Plan d'action
1. Démarrer le service de génération audio sur KXKM-AI.
   - run: ssh kxkm@kxkm-ai 'cd /data/zacus-audio && docker compose up -d musicgen'
2. Mesurer le temps de génération sur un clip de 30 s.
   - run: python3 tools/ai/audio_gen_bench.py --duration 30 --max-time 10
3. Vérifier le cache et le streaming vers l'ESP32.
   - run: curl -s http://kxkm-ai:5600/audio/generate -d '{"prompt":"mysterious lab ambient","duration":30}'
