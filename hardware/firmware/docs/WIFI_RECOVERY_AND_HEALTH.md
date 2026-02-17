# Recovery WiFi/AP & Health Check

## Objectif
Fournir une procédure claire pour diagnostiquer, rétablir et valider la visibilité du point d'accès (AP) ESP32 et la santé WiFi/RTOS sur Story V2.

---

## 1. Symptômes courants
- L'ESP32 n'apparaît pas comme point d'accès WiFi (AP) sur les appareils.
- Impossible de joindre l'API HTTP ou WebSocket (timeout, refus de connexion).
- Le moniteur série affiche : `mode=U_LOCK[BOOT_PROTO] Attente touche: K1..K6 pour lancer U_LOCK ecoute.`

---

## 2. Procédure Recovery rapide

### a) Vérification physique
- Vérifier l'alimentation (USB stable, pas de coupure).
- Vérifier le câblage et la présence du module ESP32.
- Appuyer sur un des boutons K1..K6 pour sortir du mode U_LOCK si affiché sur le moniteur série.

### b) Vérification logicielle
- Redémarrer l'ESP32 (reset physique ou via cockpit.sh flash).
- Observer le moniteur série pour tout message d'erreur ou de boot anormal.
- Si le message U_LOCK persiste, vérifier que le firmware est bien flashé (voir QUICKSTART).

### c) Scan WiFi/AP
- Depuis un PC ou smartphone, scanner les réseaux WiFi disponibles.
- Le SSID attendu doit contenir "ZACUS" ou un nom personnalisé (voir config firmware).
- Si absent, refaire un flash du firmware.

### d) Healthcheck automatisé (à implémenter)
- Script à lancer : `tools/dev/healthcheck_wifi.sh` (à créer)
- Ce script doit :
  - Scanner les AP visibles
  - Pinger l'IP par défaut (192.168.4.1)
  - Tester l'accès HTTP à `/api/status`
  - Logger les résultats dans `artifacts/rc_live/healthcheck_<timestamp>.log`

---

## 3. Validation post-recovery
- L'AP ESP32 est visible et joignable.
- L'API HTTP `/api/status` répond (curl ou navigateur).
- Les tests automatiques (smoke, HTTP API) peuvent être relancés.

---

## 4. Liens utiles
- [QUICKSTART.md](QUICKSTART.md)
- [RTOS_WIFI_HEALTH.md](RTOS_WIFI_HEALTH.md)
- [firmware-health-baseline.md](../.github/agents/reports/firmware-health-baseline.md)
- [AGENT_BRIEFINGS.md](../.github/agents/core/agent-briefings.md)

---

## 5. TODO
- [ ] Créer le script `tools/dev/healthcheck_wifi.sh` (voir section d ci-dessus)
- [ ] Ajouter un appel automatique à ce script dans la boucle de test cockpit.sh
- [ ] Archiver tous les logs de recovery dans `artifacts/rc_live/`
