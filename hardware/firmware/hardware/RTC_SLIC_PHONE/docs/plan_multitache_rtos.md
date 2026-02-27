# Plan multitâche RTOS — Agent RTOS

## Objectif
Définir l’architecture multitâche, la synchronisation et la robustesse du firmware.

---

### Architecture multitâche
- 5 tâches principales : audio, web, batterie, bluetooth, wifi
- Priorités définies selon criticité
- Stack size adapté à chaque tâche

### Synchronisation
- Queues pour communication (ex : audio ↔ batterie)
- Mutex pour accès partagé (logs, config)
- Sémaphores pour événements (wake, OTA)

### Robustesse
- Watchdog sur tâches critiques (audio, batterie)
- Tests de stress (charge, interruption)
- Gestion des erreurs (reboot, logs)

### Plan de validation
- Tests unitaires sur chaque tâche
- Tests de communication inter-tâches
- Tests de stress multitâche

---

**Version :** 2026-02-17
