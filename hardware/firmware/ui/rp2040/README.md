
# Firmware UI RP2040 (TFT 3.5 XPT2046)

## Sommaire
- [Présentation](#présentation)
- [Installation](#installation)
- [Build & Flash](#build--flash)
- [Structure](#structure)
- [Protocole UI](#protocole-ui)
- [Dépannage](#dépannage)
- [Mise à jour](#mise-à-jour)

# Présentation


Ce firmware gère l’interface utilisateur (affichage TFT tactile, communication UART avec l’ESP32).


## Installation
- Prérequis : PlatformIO, Python 3.14
- Cloner le repo, puis :
```
cd ui/rp2040
pio run
```


## Build & Flash
- Build : `pio run`
- Flash : `pio run -t upload`


## Structure
- `src/` : code source C++
- `include/` : headers partagés (voir `ui_protocol.h`)
- `docs/` : documentation

**Les protocoles, templates, schémas et exemples STORY sont désormais centralisés dans `../../docs/protocols/`.**


## Protocole UI
Voir `../../docs/protocols/PROTOCOL.md` et `../../docs/protocols/UI_SPEC.md`


## Dépannage
- Voir UI_SPEC.md et WIRING.md
- Logs série : 115200 bauds

## Mise à jour
- Merci de garder ce README à jour lors de toute évolution majeure.

---

*Pour la documentation détaillée, voir les sections ci-dessus et le dossier [docs/protocols](../../docs/protocols/).*
