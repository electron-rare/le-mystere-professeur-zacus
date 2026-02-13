# Audit de cohérence du dépôt

Date: 2026-02-12 (mise à jour cohérence globale)
Périmètre: **contenu documentaire + structure fichiers**, sans modification firmware.

## Résumé exécutif

- Le dépôt est globalement bien structuré en 3 piliers: `kit-maitre-du-jeu/`, `printables/`, `hardware/`.
- La zone la plus mature côté exécution est le firmware ESP32/ESP8266 (documenté et outillé).
- Le principal point de cohérence identifié concernait les chemins printables documentés mais absents.
- Les images PNG printables sont désormais centralisées sous `printables/export/png/`.

## Couverture de revue

- Revue des fichiers Markdown/README du dépôt (hors licences externes inchangées).
- Vérification de cohérence des chemins documentés vs arborescence versionnée.
- Vérification des assets image versionnés.

## Analyse du contenu des fichiers

### 1) Documentation racine
- `README.md` décrit clairement le périmètre et les licences.
- `CONTRIBUTING.md` couvre les règles éditoriales et le workflow.
- `CHANGELOG.md` contient désormais une release `0.2.0` et un `Unreleased` exploitable.

### 2) Kit MJ
- Le dossier `kit-maitre-du-jeu/` est cohérent et orienté animation terrain.
- Les sous-docs couvrent script, solution, rôles, anti-chaos et stations.

### 3) Printables
- La convention de nommage est explicite.
- Le dépôt déclarait une structure `src/export` non matérialisée: ce point est désormais aligné.

### 4) Hardware
- Arborescence hardware lisible (`bom`, `wiring`, `firmware`, `enclosure`).
- Le firmware reste hors périmètre de modification dans ce cycle.

## Analyse des images

Commande utilisée:
- `rg --files | rg -i '\\.(png|jpe?g|gif|webp|svg|bmp)$'`

Résultat:
- Des previews PNG sont bien versionnées, avec un classement thématique dans `printables/export/png/`.

Conséquence:
- La revue visuelle des imprimables est possible directement depuis le dépôt.
- Recommandé: maintenir ce classement à chaque nouvel export.

## Corrections de cohérence appliquées

1. Création de la structure printables attendue:
   - `printables/src/`
   - `printables/export/pdf/`
   - `printables/export/png/`
2. Création des sous-dossiers invitations cohérents avec son README:
   - `printables/invitations/src/`
   - `printables/invitations/export/pdf/`
   - `printables/invitations/export/png/`
3. Ajout de README ciblés dans les nouveaux dossiers printables.
4. Regroupement des PNG du dossier `printables/` vers `printables/export/png/` avec sous-dossiers (`general`, `fiche-enquete`, `personnages`, `zones`).

## Recommandations immédiates (hors firmware)

- Ajouter 1 à 3 documents pilotes dans `printables/export/pdf/` et leurs previews dans `printables/export/png/`.
- Maintenir `CHANGELOG.md` à chaque PR doc-only.
- Utiliser la checklist PR de `CONTRIBUTING.md` comme gate de merge.
