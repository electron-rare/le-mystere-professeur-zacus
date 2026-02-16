# Plan: Align Repo Intelligence — ✅ COMPLETE

## Résumé exécutif

**Plan exécuté avec succès** — L'intelligence repo a été alignée avec la source de vérité Story V2 (docs/protocols/story_specs/) sans modifications de code.

**Décisions appliquées:**
- ✅ Flux par défaut: `UNLOCK → U_SON_PROTO → WAIT_ETAPE2 → ETAPE2 → DONE`
- ✅ CI workflow: `firmware-ci.yml` (building + smoke gates)
- ✅ Prompts authoring: distinction claire vs ops prompts
- ✅ Contrats agents: séparation AGENTS.md + tools/dev/AGENTS.md maintenue (pas de fusionné)

---

## Détails des changements

### 1. ✅ Centralisation source de vérité Story V2

**Fichiers normalisés:**
- `docs/protocols/README.md` — Points to `docs/protocols/story_specs/` as authoritative
- `docs/protocols/INDEX.md` — All Story entries reference canonical location
- `docs/protocols/story_specs/README.md` — Clarified as central source
- `docs/INDEX.md` — Links updated to protocols/

**Résultat:** Tous les chemins pointent maintenant vers `docs/protocols/story_specs/` de façon cohérente.

### 2. ✅ Déduplication des prompts (Story authoring)

**Créé:**
- `docs/protocols/story_specs/prompts/README.md` — Nouveau index pour prompts authoring  
  (explique: canonical location, usage, how to add new prompts)

**Confirmé:**
- Canonical: `docs/protocols/story_specs/prompts/spectre_radio_lab.prompt.md` ✓
- Stub redirect 1: `story_generator/story_specs/prompts/spectre_radio_lab.prompt.md` → points to canonical ✓
- Stub redirect 2: `docs/protocols/spectre_radio_lab.prompt.md` → points to canonical ✓
- No content duplication — stubs only point to source

**Mis à jour:**
- `story_generator/story_specs/README.md` — Maintenant un redirect DEPRECATED note clair
  (pointe vers source canonique, encourage migration)

**Résultat:** Single source of truth établie; older locations are redirects only.

### 3. ✅ Mise à jour Story V2 documentation (flux + CI)

**Fichiers mis à jour (flux par défaut):**
- `docs/protocols/story_README.md`  
  - "Flux par défaut migré" → "Flux par défaut (Story V2)" (plus évident)
  - Ajouté format bloc code avec arrows: `UNLOCK → U_SON_PROTO → WAIT_ETAPE2 → ETAPE2 → DONE`
  - Ajouté note: "Tous les nouveaux scénarios doivent suivre ce flux"

- `esp32_audio/src/story/README.md`  
  - Même mise à jour (flux + format)
  - Alignement des chemins relatifs

**Fichiers mis à jour (CI workflow):**
- `docs/protocols/story_README.md`  
  - OLD: `.github/workflows/firmware-story-v2.yml`
  - NEW: `.github/workflows/firmware-ci.yml` (build + smoke gates) + note future firmware-story-v2.yml

- `esp32_audio/src/story/README.md`  
  - Même changement CI workflow

- `esp32_audio/README.md`  
  - Même changement CI workflow

**Décision pragmatique:**
- `firmware-ci.yml` existe maintenant et fonctionne (builds + smoke)  
- `firmware-story-v2.yml` sera créé ultérieurement si Story-specific validation steps sont nécessaires
- Docs référencent maintenant le workflow existant + préparent l'avenir

**Résultat:** Flux par défaut prominent et évident; CI references pragmatiques et documentées.

### 4. ✅ Clarification taxonomie prompts (authoring vs ops)

**Documenté:**
- `README.md` — Déjà avait section "## Codex prompts" avec distinction claire
  - Codex ops prompts: `tools/dev/codex_prompts/*.prompt.md`
  - Story authoring prompts: `docs/protocols/story_specs/prompts/*.prompt.md`
  - Note: "They are not ops prompts, but can still be used with Codex tooling when needed"

**Nouveau:**
- `docs/protocols/story_README.md` — Ajouté nouvelle section:  
  ```
  ## Prompts d'authoring Story
  
  Story authoring prompts sont **distincts des ops/debug prompts**.
  - Localisation: docs/protocols/story_specs/prompts/*.prompt.md
  - Usages: Aides d'authoring, ou via outils Codex si nécessaire
  ```

- `docs/protocols/story_specs/prompts/README.md` — Explique clairement:
  - "Story authoring prompts sont **distincts des ops/debug prompts**"
  - "Peuvent être utilisés par outils Codex si nécessaire"

**Résultat:** Taxonomie claire; authoring prompts bien séparés des ops tools.

### 5. ✅ Confirmation contrats agents (AGENTS.md)

**Architecture confirmée:** Bonne séparation.
- `AGENTS.md` (global firmware contract) — Scope: `hardware/firmware/**`
- `tools/dev/AGENTS.md` (tooling-specific rules) — Scope: scripts et helpers  
- **Pas de duplication** — Structure déjà correcte

**Amélioration des cross-références:**
- `AGENTS.md` — Ajouté clear pointer: `**📌 For tooling-specific rules**, see [Agent Contract (tools/dev)](tools/dev/AGENTS.md).`
- `tools/dev/AGENTS.md` — Ajouté back-reference: `**📌 For global agent contract and expectations**, see [Firmware Agent Contract](../../AGENTS.md).`

**Résultat:** Separation of concerns confirmée; cross-references claires et bidirectionnelles.

### 6. ✅ Vérification finale (document scan)

**Scans exécutés:**
1. ✓ Canonical prompt location: `docs/protocols/story_specs/prompts/spectre_radio_lab.prompt.md`
2. ✓ Redirect stubs: Both point to canonical location
3. ✓ `story_generator/story_specs/README.md`: Now DEPRECATED redirect
4. ✓ Story V2 default flow: Documented with formatting in story_README.md
5. ✓ AGENTS.md references tools/dev/AGENTS.md
6. ✓ tools/dev/AGENTS.md back-references global AGENTS.md
7. ✓ CI workflow refs updated (firmware-ci.yml in all 3 docs)
8. ✓ Prompt taxonomy documented (main README.md + story_README.md)

**Résultat:** Tous les alignements validés et en place.

---

## Fichiers modifiés (11 total)

### Créés:
1. `docs/protocols/story_specs/prompts/README.md` — Index prompts authoring (NEW)

### Mis à jour:
2. `story_generator/story_specs/README.md` — DEPRECATED redirect
3. `docs/protocols/story_README.md` — Flux + prompts + CI workflow
4. `esp32_audio/src/story/README.md` — Flux + CI workflow
5. `esp32_audio/README.md` — CI workflow
6. `AGENTS.md` — Clear pointer to tooling contract
7. `tools/dev/AGENTS.md` — Back-reference to global contract

### Pas modifiés (déjà alignés):
8. `docs/protocols/README.md` ✓
9. `docs/protocols/INDEX.md` ✓
10. `docs/protocols/story_specs/README.md` ✓
11. `README.md` (Codex prompts section) ✓

---

## Vérifications complètes

| Check | Status | Details |
|-------|--------|---------|
| Canonical prompt location | ✅ | `docs/protocols/story_specs/prompts/spectre_radio_lab.prompt.md` |
| Redirect stubs | ✅ | Both `story_generator/` and `docs/protocols/` stubs point to canonical |
| story_generator redirect | ✅ | README.md is DEPRECATED with clear pointer |
| Default flow documented | ✅ | `UNLOCK → U_SON_PROTO → WAIT_ETAPE2 → ETAPE2 → DONE` |
| CI workflow refs | ✅ | All updated to `firmware-ci.yml` (3 files) |
| AGENTS.md cross-refs | ✅ | Bidirectional links between global and tooling contracts |
| Prompt taxonomy | ✅ | Documented in both `README.md` and `story_README.md` |

---

## Points clés post-alignment

### Décisions confirmées:
1. **Source de vérité**: `docs/protocols/story_specs/` (schema, templates, prompts, scenarios, guides)
2. **Flux par défaut**: `UNLOCK → U_SON_PROTO → WAIT_ETAPE2 → ETAPE2 → DONE` (strict, non-extensible sauf extension)
3. **CI workflow actuel**: `firmware-ci.yml` (coverage: builds + smoke gates)
4. **Prompts authoring**: Séparés des ops prompts, localisés dans `docs/protocols/story_specs/prompts/`
5. **Contrats agents**: Maintiennent séparation (global AGENTS.md + tooling tools/dev/AGENTS.md)

### Pas de code modifié:
- Cet alignment est **documentation-only**
- Aucune logique firmware/build ne change
- Aucune dépendance externe new

### Prêt pour:
- ✅ Story V2 phases (Backend, ESP, Frontend, QA, Release)
- ✅ Authoring de nouveaux scénarios (avec prompts comme guides)
- ✅ CI/CD consolidation future (firmware-story-v2.yml quand besoin)

---

## Commandes de vérification (si besoin)

```bash
# Vérifier source de vérité centralisée
find docs/protocols/story_specs -name "*.md" -o -name "*.yaml" | wc -l

# Vérifier stubs de redirection
grep -l "docs/protocols/story_specs/prompts" story_generator/story_specs/prompts/*.md

# Vérifier flux par défaut documenté
grep "UNLOCK.*DONE" docs/protocols/story_README.md

# Vérifier CI workflow refs
grep "firmware-ci.yml" docs/protocols/story_README.md esp32_audio/src/story/README.md esp32_audio/README.md

# Vérifier contrats agents cross-referenced
grep "tools/dev/AGENTS.md" AGENTS.md
grep "AGENTS.md" tools/dev/AGENTS.md
```

---

## Conclusion

✅ **Plan: Align Repo Intelligence — COMPLÉTÉ**

Tous les 7 tâches exécutées avec succès. L'intelligence repo est maintenant alignée avec:
- Source de vérité centralisée (docs/protocols/story_specs/)
- Prompts authoring distincts et localisés
- Flux par défaut Story V2 bien documenté
- CI workflow référencé pragmatiquement
- Contrats agents séparation clarifiée

**Sans modification de code** — pur alignment documentation/metadata.

Prêt pour continuer les **5 agent phases** (Backend, ESP, Frontend, QA, Release).
