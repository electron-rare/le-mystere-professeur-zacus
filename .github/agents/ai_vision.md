# Custom Agent – AI Vision

## Scope
On-device object detection, player counting, and puzzle prop recognition on ESP32 camera modules.

## Technologies
- ESP-DL v3.2, ESPDet-Pico (lightweight detector)
- ESP-WHO (face/person detection framework)
- KXKM-AI (RTX 4090) for model training and quantization

## Do
- Train custom ESPDet-Pico model on puzzle prop dataset (cards, tokens, keys).
- Integrate ESP32-CAM capture pipeline with detection inference loop.
- Expose detection results via local JSON API for puzzle trigger hooks.
- Quantize models to INT8 for ESP32-S3 deployment (PSRAM-aware).
- Maintain a labeled dataset under `data/vision/` with version tags.

## Must Not
- Stream raw camera frames off-device unless debugging (bandwidth + privacy).
- Commit model weights to git; store in releases or object storage.

## Dependencies
- ESP32-CAM hardware — OV2640/OV5640 sensor, PSRAM.
- KXKM-AI node — GPU training and INT8 quantization pipeline.

## Test Gates
- Detection throughput > 7 FPS on ESP32-S3 with PSRAM.
- Accuracy > 85% mAP on the prop test set.

## References
- ESP-DL: https://github.com/espressif/esp-dl
- ESP-WHO: https://github.com/espressif/esp-who

## Plan d'action
1. Lancer l'entraînement sur KXKM-AI.
   - run: ssh kxkm@kxkm-ai 'cd /data/zacus-vision && python3 train_espdet.py --epochs 50'
2. Quantiser le modèle en INT8.
   - run: python3 tools/ai/quantize_model.py --format esp-dl --precision int8
3. Valider le FPS et la précision sur le firmware.
   - run: python3 tools/dev/vision_bench.py --min-fps 7 --min-map 0.85
