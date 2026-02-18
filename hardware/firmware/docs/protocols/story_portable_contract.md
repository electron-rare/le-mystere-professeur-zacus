# Story Portable Contract

## Objective

Runtime story loading must be portable across board layouts and support two sources:

1. LittleFS bundle (`/story/...`)
2. Generated fallback (`generatedScenarioById`)

Order is controlled by `StoryPortableConfig`.

## Runtime filesystem contract

Expected LittleFS tree:

```text
/story/
  scenarios/<SCENARIO_ID>.json
  apps/<APP_BINDING_ID>.json
  screens/<SCREEN_ID>.json
  audio/<AUDIO_PACK_ID>.json
  actions/<ACTION_ID>.json
  manifest.json
```

For each JSON resource, an adjacent checksum file is expected:

```text
<resource>.sha256
```

Example:

```text
/story/scenarios/DEFAULT.json
/story/scenarios/DEFAULT.sha256
```

## Runtime API contract

- `StoryPortableRuntime::begin(nowMs)`
- `StoryPortableRuntime::loadScenario(id, nowMs, source)`
- `StoryPortableRuntime::update(nowMs)`
- `StoryPortableRuntime::stop(nowMs, source)`
- `StoryPortableRuntime::snapshot(enabled, nowMs)`

Snapshot fields consumed by tooling:

- `runtimeState`: `idle|running|error`
- `scenarioId`
- `stepId`
- `running`
- `scenarioFromLittleFs`
- `lastError`

## Fallback policy

`StoryPortableConfig`:

- `preferLittleFs=true`: try LittleFS first.
- `strictFsOnly=true`: fail if missing in FS, no generated fallback.
- `allowGeneratedFallback=true`: fallback to generated scenarios when FS load fails.

## Build-time content source

Single source of truth for authoring:

- `game/scenarios/*.yaml`

Story spec pipeline source:

- `docs/protocols/story_specs/scenarios/*.yaml`
