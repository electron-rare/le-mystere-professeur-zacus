# Sprint Plan: AI Integration — Zacus

## Sprint 1 (Week 1-2): Security + Voice Foundation
- [ ] Deploy auth middleware to main.cpp (integrate security/ module)
- [ ] Order INMP441 microphone module
- [ ] Deploy Piper TTS Docker on VM (docker-compose.tts.yml)
- [ ] Test Piper TTS French voice quality
- [ ] Contact Espressif for "Professeur Zacus" wake word training
- [ ] Set up ESP-IDF dev environment alongside Arduino

## Sprint 2 (Week 2-3): Voice Pipeline Alpha
- [ ] Integrate ESP-SR WakeNet with placeholder wake word
- [ ] Implement WebSocket audio streaming (ESP32 → mascarade)
- [ ] Create mascarade voice bridge endpoint
- [ ] Test mic → server → TTS → speaker round-trip
- [ ] Measure end-to-end latency (target: <2s)

## Sprint 3 (Week 3-4): Vision + Hints
- [ ] Deploy ESP-DL v3.2 on ESP32-S3
- [ ] Train custom object detection model (3 puzzle props)
- [ ] Implement LLM hint endpoint on mascarade
- [ ] Create prompt library for Professor Zacus NPC
- [ ] Integrate analytics event tracking

## Sprint 4 (Week 4): Integration + Polish
- [ ] Deploy XTTS-v2 on KXKM-AI for voice cloning
- [ ] Record Professor Zacus 10s voice sample
- [ ] Full voice pipeline with cloned voice
- [ ] AudioCraft ambient generation test
- [ ] End-to-end game playtest with AI features
