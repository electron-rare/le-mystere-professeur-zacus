# Orchestration ZeroClaw (Zacus)

Dernière mise à jour: 2026-02-21.

## Objectif

Fiabiliser les sessions Zacus avec cartes branchées:

- préflight USB explicite avant upload/flash/tests live,
- session agent ciblée `zacus`,
- boucle courte issue -> PR reproductible.

## Préflight hardware

Commande dédiée:

```bash
./tools/dev/zacus.sh zeroclaw-preflight --require-port
```

Commande directe:

```bash
./tools/dev/zeroclaw_hw_preflight.sh --require-port
```

## Conversation agent ciblée Zacus

Depuis `Kill_LIFE`:

```bash
tools/ai/zeroclaw_dual_chat.sh zacus -m "fais un état hardware et propose 3 actions"
```

Si credentials provider absents:

- `zeroclaw auth login --provider openai-codex --device-code`
- ou `export OPENROUTER_API_KEY=... && export ZEROCLAW_PROVIDER=openrouter`

## Boucle recommandée

1. `zacus.sh zeroclaw-preflight --require-port`
2. prompt court Zacus
3. patch minimal
4. validation locale ciblée
5. PR Zacus avec liens d’artefacts
