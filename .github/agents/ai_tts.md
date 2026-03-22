# Custom Agent – AI TTS / Voice Cloning

## Scope
Server-side text-to-speech, Professor Zacus voice cloning, and audio streaming to ESP32 devices.

## Technologies
- Coqui XTTS-v2 (voice cloning), Piper TTS (fast fallback)
- Docker deployment on mascarade stack
- PCM/Opus streaming over HTTP chunked transfer

## Do
- Prepare and curate voice samples for Professor Zacus persona (≥ 30 s clean audio).
- Create Docker Compose service (`zacus-tts`) integrated with mascarade stack.
- Expose REST API: `POST /tts/generate` (text → audio), `POST /tts/stream` (chunked).
- Implement audio format conversion (WAV → PCM 16-bit 16 kHz) for ESP32 I2S playback.
- Cache frequently used phrases to reduce GPU load.

## Must Not
- Store voice samples in git; keep them in object storage or Docker volumes.
- Bypass mascarade auth on the TTS API endpoints.

## Dependencies
- mascarade Docker stack — networking, auth, service registry.
- ESP32 audio system — I2S DAC output and buffer management.

## Test Gates
- Latency < 2 s for a 10-word sentence (first token to last byte).
- Voice similarity > 80% (speaker verification cosine similarity).

## References
- Coqui XTTS-v2: https://github.com/coqui-ai/TTS
- Piper TTS: https://github.com/rhasspy/piper

## Plan d'action
1. Construire et démarrer le service TTS Docker.
   - run: docker compose -f docker-compose.ai.yml up -d zacus-tts
2. Vérifier la latence de génération.
   - run: curl -w '%{time_total}' -X POST http://localhost:5500/tts/generate -d '{"text":"Bonjour explorateurs"}'
3. Valider la similarité vocale sur les échantillons de référence.
   - run: python3 tools/ai/tts_similarity_bench.py --threshold 0.80
