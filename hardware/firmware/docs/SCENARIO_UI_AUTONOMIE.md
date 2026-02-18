
# Spécification : Autonomie et navigation dynamique des écrans UI

## Objectif
Permettre à l’UI (RP2040/ESP8266) de piloter toute la logique de scénario localement, en naviguant dynamiquement entre les écrans/scènes décrits dans les fichiers JSON du LittleFS, sans dépendre d’un flux imposé par l’ESP32.

## Architecture cible

- Tous les écrans/scènes sont décrits dans `/screens/<id>.json` (LittleFS).
- Un moteur de navigation local gère l’état courant, les transitions, et les actions utilisateur.
- Les transitions sont définies dans chaque fichier d’écran (ex : boutons, timeout, conditions).
- L’UI charge dynamiquement le fichier JSON correspondant à l’écran/scène à afficher.
- L’ESP32 peut envoyer un ID d’écran à afficher, mais l’UI peut aussi progresser seule selon sa logique.

## Format minimal d’un écran/scène

```json
{
  "id": "SCENE_READY",
  "content": {
    "description": "Ecran prêt, fin du scénario"
  },
  "actions": [
    { "event": "button_ok", "goto": "SCENE_WIN" },
    { "event": "timeout", "delay": 10000, "goto": "SCENE_LOCKED" }
  ]
}
```

- `actions[]` décrit les transitions possibles : bouton, timeout, condition, etc.
- `goto` indique l’ID du prochain écran à charger.

## Fonctionnement du moteur de navigation

1. Au boot, l’UI charge un écran initial (ex : `SCENE_LOCKED`).
2. À chaque événement (bouton, timer, etc.), le moteur cherche une action correspondante dans le JSON courant.
3. Si une action `goto` est trouvée, l’UI charge le fichier JSON cible et l’affiche.
4. Si aucun fichier n’est trouvé, fallback ou message d’erreur.
5. L’ESP32 peut forcer une navigation en envoyant un ID d’écran (optionnel).

## Bonnes pratiques

- Garder la structure `/screens/` identique sur toutes les cibles.
- Documenter les événements supportés (`button_ok`, `button_next`, `timeout`, etc.).
- Prévoir un fallback si un écran est absent ou corrompu.
- Synchroniser les fichiers JSON à chaque build/flash.

## À prévoir

- Script de synchro LittleFS UI (déjà présent).
- Implémentation du moteur de navigation dynamique dans le firmware UI (RP2040 et ESP8266).
- Mise à jour de la doc onboarding UI.
