# Zacus Hub — SwiftUI app (macOS + iOS)

Foundation scaffolded 2026-05-24. Three modes: **Game-master**, **Companion**, **Studio**.
See `docs/specs/2026-05-24-zacus-hub-app.md`.

## Generate the Xcode project

```bash
brew install xcodegen           # one-time
cd apps/zacus-hub
xcodegen generate
open ZacusHub.xcodeproj
```

XcodeGen reads `project.yml` and produces a multiplatform `.xcodeproj`
targeting iOS 17 + macOS 14. `ZacusHub.xcodeproj/` is git-ignored —
regenerate after editing `project.yml` or adding source files in new
directories.

## Layout

```
apps/zacus-hub/
├── project.yml                 XcodeGen spec
├── Resources/
│   ├── Info.plist              shared (iOS leaves macOS keys harmless)
│   └── ZacusHub.entitlements   sandbox + network + mic + camera
└── Sources/
    ├── App/                    @main, RootView (tabs/sidebar), Settings
    ├── Shared/                 HubSession, HubAPI (actor), Keychain
    ├── GameMaster/             operator view (sprint 1+)
    ├── Companion/              player voice + hints (sprint 3+)
    └── Studio/                 YAML browser (sprint 5+)
```

All sources live in one app target — Swift sees them as a single module,
so SourceKit errors before `xcodegen generate` are expected noise.

## Configure at first launch

1. Start the gateway (`tools/zacus-gateway/README.md`), copy the token.
2. In the app: gear icon → paste **URL** (e.g. `http://studio:8400`) +
   **token**. Saved to Keychain (`cc.saillant.zacus.hub`).
3. The Settings sheet shows live auth state (`/v1/auth/ping`).

## Sprint 0 status

| Surface | State |
|---------|-------|
| App skeleton (3 modes, settings, auth check) | ✅ scaffolded |
| Gateway connection | ✅ wired |
| Game-master state refresh | ✅ pull (stub data) |
| Companion hint request | ✅ wired to gateway proxy |
| Studio scenario browse | ✅ list + read |
| Push-to-talk, live WS, scenario edit | ⏳ later sprints |

## Conventions

- Swift 5.10, strict concurrency on.
- All network calls go through `HubAPI` actor — never raw URLSession in views.
- View files own no network state — they read from `HubSession` or pass
  via `@State` after awaiting an actor call.
- French for user-visible strings, English for code/comments.
