# Recommandations Sprint - Firmware

Date: 15 février 2026  
Branche active: `main` (toutes les fusions se font via PR/merge sur `main`)  
Contexte: Post-refactor, pre-merge PR #86

## Résumé exécutif

**État actuel :** ✅ STABLE, prêt pour merge  
**Debt technique :** Modérée (tests unitaires, docs partielles)  
**Blockers :** Aucun  
**Action immédiate :** Commit cleanup + Merge PR #86

---

## Sprint Immédiat (cette semaine)

### #1 - Commit structure cleanup ⚡ URGENT

**Raison :** 19 fichiers modifiés non committés (cleanup symlink esp32, paths)

**Action :**
```bash
cd hardware/firmware
git add -A
git commit -m "chore(firmware): clean structure

- Remove esp32 → esp32_audio symlink (caused doc confusion)
- Remove duplicate hardware/firmware/hardware/ directory
- Fix paths in AGENTS.md, READMEs, tools/qa docs
- Update port mapping (20-6.4.1/20-6.4.2)
- Extend ESP8266 OLED ready marker timeout"
```

**Validation :**
```bash
git status  # should show "nothing to commit, working tree clean"
./build_all.sh  # confirm builds still pass
```

**Durée estimée :** 10 min  
**Priorité :** 🔥 CRITIQUE

---

### #2 - Merge PR #86 ⚡ URGENT

**Raison :** 42 commits ahead of main, architecture refactor complete

**Pre-merge checklist :**
- [x] Builds passing (5/5 environments)
- [x] Smoke tests hardware OK
- [x] Structure cleanup done (#1)
- [ ] Review finale docs (QUICKSTART, ARCHITECTURE_UML)
- [ ] Decision squash vs merge (42 commits)
- [ ] Tag version post-merge

**Action :**
```bash
# Review PR #86 on GitHub
# https://github.com/electron-rare/le-mystere-professeur-zacus/pull/86

# Option A : Merge commit (historique complet)
git checkout main
git merge hardware/firmware
git push origin main

# Option B : Squash (historique propre, recommandé)
# Via GitHub UI : "Squash and merge"
# Message: "feat(firmware): complete architecture refactor v2"
```

**Post-merge :**
```bash
git tag v0.9.0-beta
git push origin v0.9.0-beta
```

**Durée estimée :** 30 min (review) + 15 min (merge + tag)  
**Priorité :** 🔥 CRITIQUE

---

### #3 - Tests hardware end-to-end

**Raison :** Valider story flow complet UNLOCK -> WIN -> WAIT_ETAPE2 -> ETAPE2 -> DONE

**Scénarios à tester :**

#### 3.1 Happy path

1. Boot ESP32 + ESP8266
2. UI Link handshake OK
3. Story mode auto-start
4. ETAPE1 : Play intro, wait LA detection (simulate 440Hz input)
5. Transition unlock : 6-frame animation OLED
6. ETAPE2 timer : Wait 15 min (ou force avec debug command)
7. MP3 gate open : SD catalog playback OK
8. Test volume up/down, next/prev tracks

**Verdict attendu :** ✅ Tout passe, aucun panic, UI cohérent

#### 3.2 Failure paths

1. **SD card missing**
   - Attendu : Fallback FX I2S (beep, sweep)
   - Verdict : ✅ Audio continue (dégradé mais fonctionnel)

2. **UI Link disconnect pendant runtime**
   - Action : Débrancher câble ESP32↔ESP8266
   - Attendu : ESP32 continue story, ESP8266 affiche "LINK DOWN"
   - Action : Rebrancher câble
   - Attendu : HELLO handshake auto, KEYFRAME resync

3. **Audio MP3 corrupt**
   - Action : Corrompre fichier MP3 sur SD
   - Attendu : Timeout 30s, fallback FX

4. **Long run (4h)**
   - Action : Laisser ESP32 runtime 4h en story mode
   - Attendu : Heap stable, aucun reboot, UI Link uptime >99%

**Durée estimée :** 2h total (happy path 30 min, failure paths 1h30)  
**Priorité :** 🔥 HAUTE

---

## Sprint Court Terme (2 semaines)

### #4 - Tests unitaires services critiques

**Raison :** Coverage ~5%, régression risquée sur refactor futur

**Targets prioritaires :**

| Module | Tests critiques | Effort |
|--------|----------------|--------|
| UiLink | Frame parsing, CRC validation, timeout recovery | 3h |
| StoryEngine | Transitions, event queue, jump step | 4h |
| AudioService | Channel switch, timeout, fallback | 3h |
| InputService | Debounce, long-press, repeat | 2h |

**Framework :** Unity (PlatformIO native test)

**Structure :**
```
hardware/firmware/
└── test/
    ├── test_ui_link/
    │   ├── test_frame_parsing.cpp
    │   ├── test_crc_validation.cpp
    │   └── test_reconnection.cpp
    ├── test_story_engine/
    │   ├── test_transitions.cpp
    │   ├── test_event_queue.cpp
    │   └── test_jump_step.cpp
    └── test_audio_service/
        ├── test_channel_switch.cpp
        └── test_timeout_fallback.cpp
```

**Commande :**
```bash
pio test -e native  # run tests on host (no hardware)
```

**Durée estimée :** 12h  
**Priorité :** 🟡 MOYENNE

---

### #5 - Documentation services complets

**Raison :** Docs inégales, onboarding dev ralenti

**Actions :**

1. **Standardiser headers Doxygen**
   - Template :
     ```cpp
     /**
      * @brief Short description
      * 
      * Detailed description explaining:
      * - What the service does
      * - Dependencies required
      * - Thread-safety notes if relevant
      * 
      * @example
      * AudioService audio;
      * audio.startBaseFs(SD, "/intro.mp3", 0.8f, 30000, "intro");
      */
     class AudioService {
       // ...
     };
     ```

2. **Diagrammes séquence flows complexes**
   - Story transition typical flow
   - UI Link reconnection
   - Audio channel switch + fallback

3. **README chaque service/**
   - services/audio/README.md
   - services/input/README.md
   - services/network/README.md
   - etc. (10 total)

**Durée estimée :** 8h  
**Priorité :** 🟡 MOYENNE

---

### #6 - Optimisation mémoire ESP8266

**Raison :** Heap free ~35KB (45% usage), risque si UI frames complexes

**Actions :**

1. **Profiling heap usage**
   ```cpp
   // Ajouter dans main loop ESP8266
   Serial.printf("[HEAP] Free: %u bytes\n", ESP.getFreeHeap());
   ```

2. **Identifier pics allocation**
   - UI frame buffer LVGL (si RP2040 envoie gros KEYFRAME)
   - String concatenation dans frame parser

3. **Optimisations candidates**
   - Pooler frame buffers (réutiliser au lieu de malloc/free)
   - Limiter taille max frame (320 bytes déjà OK, valider)
   - Réduire fréquence render OLED (250ms → 500ms si heap<25KB)

**Validation :**
```bash
# Test stress : envoyer 100 KEYFRAME frames/sec pendant 1 min
# Heap ESP8266 doit rester >20KB
```

**Durée estimée :** 4h  
**Priorité :** 🟡 MOYENNE

---

## Sprint Moyen Terme (1 mois)

### #7 - Config runtime via SD (optionnel)

**Raison :** Actuellement mode (STORY/MP3/RADIO) hardcodé config.h, recompile requis

**Design :**

**Fichier :** `SD:/config.json`
```json
{
  "mode": "STORY",
  "volume_default": 50,
  "story": {
    "scenario": "default_scenario",
    "etape2_delay_min": 15
  },
  "audio": {
    "base_timeout_ms": 30000,
    "overlay_timeout_ms": 5000
  },
  "ui_link": {
    "baud": 57600,
    "timeout_ms": 1500,
    "heartbeat_ms": 1000
  }
}
```

**Implémentation :**
```cpp
// app_orchestrator.cpp
void AppOrchestrator::loadConfig() {
  if (SD.exists("/config.json")) {
    File f = SD.open("/config.json", "r");
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, f);
    g_config.mode = doc["mode"].as<String>();
    g_config.volumeDefault = doc["volume_default"];
    // ...
  } else {
    // Fallback to config.h defaults
  }
}
```

**Avantages :**
- Switch mode sans rebuild
- Tweaker timeouts sur terrain
- Backup config easy (copy SD)

**Inconvénients :**
- Complexité +1
- SD requis (fallback config.h OK)

**Durée estimée :** 6h  
**Priorité :** 🟢 BASSE

---

### #8 - CI/CD GitHub Actions

**Raison :** Builds manuels, pas de check automatique PR

**Design :**

**Fichier :** `.github/workflows/firmware-ci.yml`
```yaml
name: Firmware CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        env: [esp32dev, esp32_release, esp8266_oled, ui_rp2040_ili9488, ui_rp2040_ili9486]
    steps:
      - uses: actions/checkout@v3
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: 3.11
      - name: Install PlatformIO
        run: pip install platformio
      - name: Build ${{ matrix.env }}
        run: |
          cd hardware/firmware
          pio run -e ${{ matrix.env }}
      - name: Upload artifacts
        uses: actions/upload-artifact@v3
        with:
          name: firmware-${{ matrix.env }}
          path: hardware/firmware/.pio/build/${{ matrix.env }}/firmware.*
```

**Badges README :**
```markdown
[![Firmware CI](https://github.com/electron-rare/le-mystere-professeur-zacus/actions/workflows/firmware-ci.yml/badge.svg)](https://github.com/electron-rare/le-mystere-professeur-zacus/actions/workflows/firmware-ci.yml)
```

**Durée estimée :** 3h  
**Priorité :** 🟢 BASSE (mais haute valeur ajoutée)

---

### #9 - Refactor globals vers DI pur (long terme)

**Raison :** Globals `g_app`, `g_services` couplent code, testabilité réduite

**Phases :**

#### Phase 1 : Services registry pattern
```cpp
class ServiceRegistry {
public:
  void registerService(const char* name, void* service);
  void* getService(const char* name);
private:
  std::map<String, void*> services_;
};

// Usage
ServiceRegistry& registry = ServiceRegistry::instance();
registry.registerService("audio", &audioService);
AudioService* audio = static_cast<AudioService*>(registry.getService("audio"));
```

#### Phase 2 : Éliminer g_app global
```cpp
// Avant
App g_app;
void setup() { g_app.setup(); }

// Après
int main() {
  ServiceRegistry registry;
  App app(registry);
  app.setup();
  while(1) app.loop();
}
```

**Avantages :**
- Testabilité +10 (mock services facile)
- Couplage réduit
- Code plus idiomatique C++

**Inconvénients :**
- Effort refactor élevé
- Risque régression

**Durée estimée :** 20h  
**Priorité :** 🟢 BASSE (long terme, pas bloquant)

---

## Checklists de qualité

### Pre-merge checklist (avant chaque merge main)

- [ ] `./build_all.sh` → 5/5 PASS
- [ ] `./tools/dev/run_matrix_and_smoke.sh` → ALL PASS
- [ ] `git status` → clean (ou commit explicite)
- [ ] Docs mises à jour (README, CHANGELOG si applicable)
- [ ] Pas de secrets committés (tokens, passwords)
- [ ] .gitignore couvre artifacts/logs

### Pre-release checklist (avant tag version)

- [ ] Tests end-to-end hardware complets (#3)
- [ ] Long run stability 4h OK
- [ ] Documentation release notes
- [ ] Backup firmware binaries (.bin archives)
- [ ] Tag version semver (ex: v0.9.0-beta)

### Code review checklist

- [ ] Naming cohérent (CamelCase classes, snake_case variables)
- [ ] Headers documented (Doxygen-style)
- [ ] Error handling explicite (Result enum, logs)
- [ ] Timeouts explicites (jamais d'attente infinie)
- [ ] Memory leaks check (heap usage stable)
- [ ] Thread-safety si applicable (locks, atomics)

---

## Métriques de succès

| Métrique | Actuel | Q1 2026 Target | Q2 2026 Target |
|----------|--------|----------------|----------------|
| Code coverage | 5% | 40% | 60% |
| Docs coverage | 70% | 85% | 95% |
| Bugs critiques | 0 | 0 | 0 |
| Heap ESP32 free | 210KB | >180KB | >180KB |
| Heap ESP8266 free | 35KB | >25KB | >25KB |
| UI Link uptime | 99.8% | >99.5% | >99.9% |
| Build time | 45s | <40s | <35s |
| Sprint velocity | - | 20 SP | 25 SP |

**Definition of Done :**
- Code merged to main
- Tests passing (smoke minimum, unitaires si applicable)
- Docs updated
- Reviewed by 1+ peer
- No regressions detected

---

## Contacts & Resources

**Team :**
- Lead firmware : @electron-rare
- QA/Testing : (à assigner)
- Docs : (à assigner)

**Links :**
- Repo : https://github.com/electron-rare/le-mystere-professeur-zacus
- Issues : https://github.com/electron-rare/le-mystere-professeur-zacus/issues
- Discussions : https://github.com/electron-rare/le-mystere-professeur-zacus/discussions

**Communication :**
- Daily updates : Commit messages + PR comments
- Blockers : GitHub Issues avec label `blocker`
- Retrospectives : Fin de sprint (2 semaines)

---

**Dernière mise à jour :** 15 février 2026  
**Prochaine review :** 1er mars 2026  
**Status :** 🚀 READY TO EXECUTE
