# Custom Agent – AI Voice (ESP-SR)

## Scope
Wake word detection, speech command recognition, and voice pipeline between ESP32 and server (mascarade bridge).

## Technologies
- ESP-SR v2.0, WakeNet, MultiNet, AFE (Acoustic Front-End)
- I2S microphone input, mascarade REST API

## Do
- Configure custom wake word ("Professeur Zacus") via WakeNet training tools.
- Implement French command vocabulary in MultiNet with game-relevant phrases.
- Wire I2S mic input through AFE → WakeNet → MultiNet pipeline on ESP32.
- Bridge recognized commands to mascarade API for LLM processing.
- Maintain detection thresholds and noise-gate parameters in a tuneable config header.

## Must Not
- Ship raw audio to server unless explicitly requested (privacy + bandwidth).
- Modify firmware I2S driver outside `hardware/firmware/` conventions; coordinate with firmware_core agent.

## Dependencies
- `firmware_core.md` — ESP32 build chain and I2S driver.
- mascarade API — command relay and LLM orchestration.

## Test Gates
- Wake word detection rate > 95% at 1 m distance, ambient < 50 dB.
- French command accuracy > 90% on the defined vocabulary set.

## References
- Espressif ESP-SR documentation: https://github.com/espressif/esp-sr
- `hardware/firmware/AGENTS.md`

## Plan d'action
1. Valider le build ESP-SR avec WakeNet activé.
   - run: bash hardware/firmware/build_all.sh
2. Tester la détection du wake word sur le banc de test.
   - run: python3 tools/dev/wake_word_bench.py --threshold 0.95 --distance 1m
3. Mesurer la précision des commandes françaises.
   - run: python3 tools/dev/command_accuracy.py --lang fr --min-accuracy 0.90
