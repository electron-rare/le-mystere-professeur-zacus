# Story Screen Palette V2

Palette de reference pour `data/story/screens/*.json` et pour la generation auto (`tools/dev/story-gen generate-bundle`).

## Structure JSON recommandee

```json
{
  "id": "SCENE_READY",
  "title": "PRET",
  "subtitle": "Scenario termine",
  "symbol": "READY",
  "effect": "pulse",
  "theme": { "bg": "#0F2A12", "accent": "#6CD96B", "text": "#EDFFED" },
  "text": {
    "show_title": false,
    "show_subtitle": true,
    "show_symbol": true,
    "title_case": "upper",
    "subtitle_case": "raw",
    "title_align": "top",
    "subtitle_align": "bottom"
  },
  "framing": {
    "preset": "center",
    "x_offset": 0,
    "y_offset": 0,
    "scale_pct": 100
  },
  "scroll": {
    "mode": "none",
    "speed_ms": 4200,
    "pause_ms": 900,
    "loop": true
  },
  "demo": {
    "mode": "standard",
    "particle_count": 4,
    "strobe_level": 65
  },
  "transition": {
    "effect": "fade",
    "duration_ms": 240
  },
  "timeline": {
    "loop": true,
    "duration_ms": 1400,
    "keyframes": [
      {
        "at_ms": 0,
        "effect": "pulse",
        "speed_ms": 680,
        "theme": { "bg": "#08152D", "accent": "#3E8DFF", "text": "#F5F8FF" }
      }
    ]
  }
}
```

## Scene IDs (source de vérité)

Source de vérité runtime: `hardware/libs/story/src/resources/screen_scene_registry.cpp` (`kScenes`).

Scènes canoniques (source unique):
- `SCENE_LOCKED`
- `SCENE_BROKEN`
- `SCENE_SEARCH`
- `SCENE_LA_DETECTOR`
- `SCENE_CAMERA_SCAN`
- `SCENE_SIGNAL_SPIKE`
- `SCENE_REWARD`
- `SCENE_MEDIA_ARCHIVE`
- `SCENE_READY`
- `SCENE_WIN`

La validation doit refuser toute `screenSceneId` hors registre.

Fallback runtime attendu en cas d'id inconnu:
- rejet côté build/revue + rejet en runtime (`SCREEN_SCENE_ID_UNKNOWN` / charge impossible),
- aucune activation de fallback silencieux côté scène.

## Palette d'effets (scene)

- `none`: scene statique.
- `pulse`: pulsation du coeur/rings.
- `scan`: balayage vertical.
- `radar`: anneaux radar + balayage.
- `wave`: onde/strobe horizontal.
- `blink`: clignotement/strobe.
- `celebrate`: mode celebration (bar + particules).

Aliases normalises au build:
- `steady -> none`
- `glitch -> blink`
- `reward -> celebrate`
- `sonar -> radar`

Fallback runtime (non silencieux):
- effet inconnu: log + `pulse`.

## Options texte

- `show_title`: bool.
- `show_subtitle`: bool.
- `show_symbol`: bool.
- `title_case`: `raw | upper | lower`.
- `subtitle_case`: `raw | upper | lower`.
- `title_align`: `top | center | bottom`.
- `subtitle_align`: `top | center | bottom`.

## Options cadrage (framing)

- `preset`: `center | focus_top | focus_bottom | split`.
- `x_offset`: int `[-80..80]`.
- `y_offset`: int `[-80..80]`.
- `scale_pct`: int `[60..140]`.

## Options scrolling (subtitle)

- `mode`: `none | marquee`.
- aliases: `ticker` et `crawl` sont normalises en `marquee`.
- `speed_ms`: int `[600..20000]`.
- `pause_ms`: int `[0..10000]`.
- `loop`: bool.

## Options demo-making

- `mode`: `standard | cinematic | arcade`.
  - `cinematic`: transitions plus lentes, moins de particules.
  - `arcade`: transitions plus rapides, tempo d'effet plus nerveux.
- `particle_count`: int `[0..4]`.
- `strobe_level`: int `[0..100]`.

## Transitions visuelles entre scenes (UI)

`transition.effect`:
- `none`
- `fade`
- `slide_left`
- `slide_right`
- `slide_up`
- `slide_down`
- `zoom`
- `glitch`

Aliases normalises au build:
- `crossfade -> fade`
- `left -> slide_left`
- `right -> slide_right`
- `up -> slide_up`
- `down -> slide_down`
- `zoom_in -> zoom`
- `flash -> glitch`
- `wipe -> slide_left`
- `camera_flash -> glitch`

`transition.duration_ms`: entier positif (fallback auto si invalide).

Fallback runtime (non silencieux):
- transition inconnue: log + fallback au transition courant de la scène.

## Triggers possibles pour passer a la scene suivante (Story Engine V2)

Les triggers de transition sont definis dans les `steps.transitions[]` des specs scenario (pas dans `screens/*.json`).

- `on_event`: transition sur reception d'un evenement.
- `after_ms`: transition apres delai.
- `immediate`: transition immediate a l'entree d'etape.

`event_type` autorises:
- `none`
- `unlock`
- `audio_done`
- `timer`
- `serial`
- `action`

Note runtime:
- l'animation visuelle `transition.*` est appliquee quand `scene_id` change.
- le choix de la prochaine etape reste pilote par `trigger + event_type + event_name + priority`.
