# Plan de tests — RTC_BL_PHONE (livraison finale)

## 1. Objectif
Valider la robustesse, la couverture fonctionnelle et la conformité de la livraison RTC_BL_PHONE : téléphonie, web, MQTT, ESP-NOW, DTMF, tests, CI/CD, documentation.

## 2. Environnements
- ESP32 DevKitC (esp32dev)
- ESP32-S3-DevKitC-1 (esp32-s3-devkitc-1)

## 3. Tests unitaires (PlatformIO/Unity)
- Lancer `pio test` sur chaque environnement
- Vérifier le passage de tous les tests (DTMF, props routing, AudioManager, SLIC, etc.)
- Générer le rapport de couverture (`scripts/gen_coverage.sh`)

## 4. Tests fonctionnels
- Appels téléphoniques (émission, réception, raccrochage, décrochage)
- Contrôle via web UI (initier appel, voir statut, contacts)
- Contrôle via MQTT (topics in/out, payload JSON)
- Contrôle via ESP-NOW (commande locale, broadcast)
- Détection DTMF logicielle (Goertzel)
- Routage audio (lecture MP3, volume, mute)
- Gestion WiFi (connexion, déconnexion, fallback)

## 5. Tests web/HTTP
- Endpoints `/`, `/status`, `/config`, `/logs` : code 200, format JSON
- Tests de charge (requêtes multiples)
- Tests d’accès non autorisé
- Vérification sécurisation API : accès POST sans authentification (401/403 attendu), test CORS, automatisé par `scenario_api_security`

## 6. Robustesse & sécurité
- Scénarios de coupure WiFi, reboot, perte agent MQTT/ESP-NOW
- Validation fallback config SPIFFS/NVS
- Logs d’erreur et de récupération

### Couverture automatisée réseau (WiFi/BLE)
- Scan WiFi/BLE : détection des réseaux et périphériques à proximité
- Connexion WiFi/BLE : test explicite de connexion (SSID, mot de passe, nom BLE)
- Coupure/rétablissement WiFi : test de déconnexion/reconnexion automatique, validation de la reprise de service
- Coupure WiFi + fallback BLE : test de bascule automatique sur BLE si WiFi indisponible

## 7. Limitations connues
- Bluetooth Classic non supporté sur ESP32-S3 (fallback BLE)
- Warnings `DynamicJsonDocument` (non bloquant, documenté)

## 8. Documentation
- Vérifier la complétude des guides README, fiches agents, rapports CI, plans de test

## 9. Critères de succès
- Tous les tests passent sur les deux cibles
- Fonctionnalités principales validées (téléphonie, web, MQTT, ESP-NOW, DTMF)
- Documentation complète et à jour
- Aucun blocage critique non documenté

---

**Version :** 2026-02-18
