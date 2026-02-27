# Protocole de test automatisé — Agent QA & Moniteur Série

## Objectif
Décrire la procédure de test automatisée et semi-automatisée pour RTC_BL_PHONE, en s’appuyant sur un agent QA dédié et l’utilisation du moniteur série pour la traçabilité et la validation.

---

## 1. Préparation
- Brancher ESP32 Audio Kit et ESP32-S3-DevKitC-1
- Flasher le firmware sur chaque carte
- Ouvrir le moniteur série à 115200 bauds
- Préparer le script de capture des logs série (ex : `screen`, `minicom`, ou script Python)

## 2. Agent QA — Rôle
- Supervise l’exécution des tests hardware
- Déclenche les scénarios (commande série, web, MQTT, ESP-NOW)
- Observe et enregistre les logs série
- Valide les critères de succès pour chaque test
- Remplit le rapport de validation en temps réel

## 3. Protocole de test
### a. Initialisation
- Vérifier le boot, la détection hardware, l’affichage du statut sur le moniteur série
- Noter toute erreur ou warning au démarrage

### b. Exécution séquentielle
Pour chaque test :
1. Déclencher l’action (commande série, web, MQTT, ESP-NOW)
2. Observer la réponse sur le moniteur série
3. Vérifier la conformité du log (statut, événement, erreur)
4. Noter le résultat dans le rapport

### c. Exemples de commandes série
- `h` : aide
- `s` : statut runtime
- `p <mac>` : configurer la MAC
- `m <numero>` : émission d’appel
- `a` : décrocher
- `e` : raccrocher
- `v <0..15>` : volume

### d. Capture automatique
- Utiliser un script pour enregistrer tous les logs série dans un fichier horodaté
- Marquer chaque début/fin de test dans le log (ex : `=== TEST AUDIO START ===`)

## 4. Validation et traçabilité
- Chaque test est validé si le log série confirme l’action attendue sans erreur
- Les logs sont archivés avec le rapport de validation
- Toute anomalie est documentée immédiatement

## 5. Agent QA — Spécialisation
- Peut être un opérateur humain, un script Python, ou un outil d’automatisation (ex : PySerial)
- Doit pouvoir envoyer des commandes, lire et parser les logs, générer un rapport automatique

---

**Version :** 2026-02-18
