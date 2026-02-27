# Gate critique — Blocage WiFiServer.h

## Description
- Blocage du build PlatformIO sur ESP32 : fatal error WiFiServer.h (WebServer)
- Origine : WebServer inclus par le framework Arduino ESP32, non compatible ou absent
- Aucun code source du projet ne dépend de WebServer, mais la bibliothèque est installée par défaut

## Actions tentées
- Retrait des dépendances tierces (audio-tools)
- Mise à jour du framework espressif32 et des bibliothèques
- Audit des dépendances installées

## Recommandation experte
- Utiliser exclusivement ESPAsyncWebServer pour tous les endpoints HTTP
- Ne pas inclure WebServer ni WiFiServer.h dans le code source
- Documenter ce gate comme critique dans la CI et la synthèse de phase
- Proposer une stratégie de contournement : tests unitaires sur les modules non dépendants du serveur HTTP

## Stratégie CI
- Valider les tests unitaires sur les modules audio, SLIC, téléphone, RTOS
- Reporter le blocage serveur HTTP dans docs/AGENT_TODO.md et docs/RC_AUTOFIX_CICD.md
- Suivre l’évolution du framework Arduino ESP32 pour correction future

---

**Version :** 2026-02-17
