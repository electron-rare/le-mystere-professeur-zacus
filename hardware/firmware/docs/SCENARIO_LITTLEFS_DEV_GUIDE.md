

# Guide développeur : scénarios LittleFS (story-V2) – Dossier `data/` unique cross-plateforme

## 1. Objectif

- **Unifier** la structure des assets LittleFS (scénarios, écrans, scènes, audio, actions, etc.) dans un dossier `data/` unique à la racine du projet firmware.
- **Garantir** que ce dossier serve de source unique pour le flash LittleFS sur ESP32, ESP8266 et RP2040.
- **Faciliter** la génération, la synchronisation et la maintenance des fichiers de scénario et de ressources.

---

## 2. Structure recommandée

```
hardware/firmware/data/
	story/
		scenarios/
			DEFAULT.json
		apps/
			app1.json
			app2.json
		screens/
			screen1.json
			screen2.json
		audio/
			audio1.json
		actions/
			action1.json
	audio/
		uson_boot_arcade_lowmono.mp3
		uson_1_radio_lowmono.mp3
		...
	radio/
		stations.json
	net/
		config.json
```

- **Tous les firmwares** (ESP32, ESP8266, RP2040) doivent pointer vers ce dossier `data/` pour la génération et le flash LittleFS.
- Les scripts de génération (ex : `gen_split_scenario.py`) doivent produire directement dans `hardware/firmware/data/story/`.
- Les assets audio, radio, net, etc. sont également centralisés dans ce dossier.

---

## 3. Intégration firmware

- **ESP32** : charge les scénarios, écrans, scènes, audio, etc. depuis `data/story/` (LittleFS).
- **ESP8266** : charge les ressources locales (écrans, assets) depuis `data/story/` (LittleFS).
- **RP2040** : charge les écrans/scénarios depuis `data/story/` (LittleFS, via LVGL ou équivalent).

---

## 4. Bonnes pratiques

- **Un seul dossier source** pour tous les assets LittleFS : `hardware/firmware/data/`
- **Scripts de génération** : toujours cibler ce dossier pour éviter la duplication.
- **Vérification automatique** : scripts et firmware doivent vérifier la présence de tous les fichiers nécessaires avant le flash/test.
- **Documentation** : mettre à jour tous les guides (`SCENARIO_LITTLEFS_DEV_GUIDE.md`, `README.md`, etc.) pour pointer vers ce dossier unique.

---

## 5. Migration

- Déplacer tous les fichiers de scénario, écrans, scènes, audio, etc. dans `hardware/firmware/data/`.
- Adapter les scripts de génération et de flash pour pointer vers ce dossier.
- Supprimer les anciens dossiers `data/` dispersés dans les sous-projets (ex : `ui/rp2040_tft/data/`, `esp32_audio/data/`).

---

## 6. Exemple de workflow

1. Génération des fichiers :
	 ```sh
	 cd hardware/firmware/story_generator/
	 python3 gen_split_scenario.py --input scenario.yaml --output_dir ../data/story/
	 ```
2. Flash LittleFS (exemple ESP32) :
	 ```sh
	 pio run -t uploadfs -e esp32dev --upload-dir=hardware/firmware/data/
	 ```

---

## 7. À mettre à jour

- `SCENARIO_LITTLEFS_DEV_GUIDE.md`
- `SCENARIO_LITTLEFS_EXAMPLE.md`
- Tous les README concernés
- Scripts de génération et de flash

---

**Résumé** :  
Un dossier `data/` unique à la racine `hardware/firmware/` devient la référence pour tous les assets LittleFS, garantissant cohérence, simplicité de maintenance et compatibilité cross-plateforme (ESP32, ESP8266, RP2040).
