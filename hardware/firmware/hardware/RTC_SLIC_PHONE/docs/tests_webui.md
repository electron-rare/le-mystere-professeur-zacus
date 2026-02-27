# Tests fonctionnels WebUI RTC_BL_PHONE

## Objectif
Valider l’affichage, le rafraîchissement des données et l’interaction du webUI embarqué.

## Scénario
- Accès à http://<ip_esp32>/
- Affichage du dashboard (état, batterie, audio, SLIC, Bluetooth, WiFi)
- Rafraîchissement via bouton (appel /api/status)
- Vérification du retour JSON et mise à jour dynamique
- Test erreur de connexion (serveur non disponible)

## Résultats attendus
- Affichage correct des données
- Rafraîchissement instantané
- Gestion des erreurs (message affiché)

---

_Agent WebUI – Tests générés automatiquement._
