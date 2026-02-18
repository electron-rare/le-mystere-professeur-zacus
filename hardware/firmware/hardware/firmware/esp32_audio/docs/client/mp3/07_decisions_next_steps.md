# Decisions et next steps apres presentation client

## Decisions figees pour cette presentation

1. Scope client: MP3 uniquement
2. Format: live + backup
3. Reponses serie canoniques strictes
4. `MP3_CAPS` dynamique runtime comme reference officielle
5. Pas de `vTask` sur ce lot

## Decisions a prendre apres demo

1. Priorite roadmap:
- extraction complete MP3 hors orchestrateur
- optimisation rendu OLED MP3 en charge
- extension observabilite backend

2. Politique backend:
- garder `AUTO_FALLBACK` par defaut
- definir eventuelle ouverture progressive capabilities tools

3. Process release:
- cadence smoke live par sprint
- synchro branches `codex -> main -> codex` imposee

## Next steps techniques proposes

1. PR MP3 V2.2:
- extraction actions clavier/serie vers `Mp3Controller`
- nettoyage final du wiring MP3 dans orchestrateur

2. PR MP3 V2.3:
- finalisation UI ESP8266 page-aware
- stabilisation anti-flicker + rendu partiel

3. PR MP3 V2.4:
- extension status backend (reasons normalisees)
- dashboard serie compact pour support terrain

4. PR MP3 V2.5:
- campagne RC live scriptable avec rapport genere
- publication artefacts de repetition (sans logs sensibles)

