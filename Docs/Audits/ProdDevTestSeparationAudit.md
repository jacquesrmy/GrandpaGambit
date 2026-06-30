# Grandpa Gambit - Prod / Dev / Debug / Sandbox / Tests Separation Audit

Date: 2026-06-30
Scope: prod/dev/test separation audit before final UI work. Pass 1 gates debug/sandbox ownership and controller exec commands for non-shipping builds only. Pass 2 renames runtime feedback/snapshot APIs away from `Debug`. No gameplay, UI-final, map, Blueprint, DataAsset, or validation script changes were made.

## 1. Statut global

### Verdict court

Risque actuel apres Pass 2: **moyen avant UI finale**.

The production runtime does **not** appear to call automation tests or test fixtures directly. Automation code is under `Source/GrandpaGambit/Private/Tests` and guarded with `WITH_DEV_AUTOMATION_TESTS`.

Pass 1 removed the unconditional production startup/debug command boundary violations:

- `AGambitGameMode` creates `UGambitMatchDebugComponent` and `UGambitDevMatchSandboxComponent` only under `!UE_BUILD_SHIPPING`.
- `AGambitPlayerController` keeps UHT-valid `UFUNCTION(Exec, meta=(DevelopmentOnly))` declarations, runs real debug/dev implementations only under `!UE_BUILD_SHIPPING`, and compiles Shipping stubs that only log unavailability.
- `UGambitDevMatchSandboxWidget::ResolveSandboxComponent()` returns `nullptr` in Shipping because the production `GameMode` no longer exposes the sandbox getter in that build.

Pass 2 removed the runtime feedback/snapshot naming pollution:

- `GambitDebugTypes.h` was replaced by `GambitRoundFeedbackTypes.h`.
- Runtime feedback now uses `FGambitRoundFeedbackEvent`, `FGambitScoreBreakdownLine`, `FGambitGoldBreakdownLine`, `FGambitShopBreakdownLine`, `FGambitDiceSnapshot`, `FGambitItemSnapshot`, and `FGambitPlayerSnapshot`.
- Runtime APIs now use stable names such as `AddRoundFeedbackEvent()`, `GetScoreBreakdownLines()`, `BuildPlayerSnapshot()`, and `BuildPlayerSnapshots()`.
- Target-selection summary copy now uses `PresentationText`.

Remaining risk before final UI comes from later passes: the default `GameInstanceClass` path under `Blueprints/Dev`, stale legacy/debug content references, and the temporary PC shell quarantine/deprecation policy.

### Est-ce que la prod appelle du test/sandbox/debug ?

| Category | Result | Notes |
| --- | --- | --- |
| Production -> Automation Tests | **No direct dependency found** | Test files are in `Private/Tests` and guarded by `WITH_DEV_AUTOMATION_TESTS`. |
| Production -> Sandbox | **No unconditional Shipping ownership after Pass 1** | `AGambitGameMode` owns `UGambitDevMatchSandboxComponent` only under `!UE_BUILD_SHIPPING`; `GambitDev...` exec commands are marked `DevelopmentOnly`, execute real sandbox logic only under `!UE_BUILD_SHIPPING`, and use Shipping stubs otherwise. |
| Production -> Debug Tools | **No unconditional Shipping ownership after Pass 1** | `AGambitGameMode` owns `UGambitMatchDebugComponent` only under `!UE_BUILD_SHIPPING`; debug/cheat/automation-like exec commands are marked `DevelopmentOnly`, execute real debug logic only under `!UE_BUILD_SHIPPING`, and use Shipping stubs otherwise. |
| Production -> Debug-named feedback types | **No runtime API exposure after Pass 2** | Runtime score, gold, shop, effect, dice, item, and player snapshots use stable feedback/snapshot names. `Source/Debug` tools remain debug-named by design. |

### Est-ce que le shell PC pollue le runtime final ?

`UGambitPCShellWidget` is best classified as **Temporary PC Shell / Production UI V0.1**, not as final UI and not as pure debug UI.

It can stay in runtime for V0.1 because the current target is PC keyboard/mouse and docs already define it as a minimal playable shell. After Pass 2 it consumes stable feedback/snapshot APIs instead of `FGambitDebug...` snapshots or `DebugText`, but it should still not become the final UI foundation without a dedicated shell quarantine/deprecation pass.

## 2. Taxonomie officielle

| Category | Definition | Allowed dependencies | Forbidden dependencies |
| --- | --- | --- | --- |
| Production Runtime | Core match, round, dice, scoring, economy, shop, inventory, shared pool, ranking, player state, game state, settings, data definitions. | Runtime systems and stable data/config. | Tests, sandbox, debug tools, dev grants, debug-only widgets/maps. |
| Production UI V0.1 | UI required to play V0.1 on PC keyboard/mouse. May be rough but must not own gameplay truth. | Stable runtime APIs, GameState/PlayerState snapshots, `Request...` commands. | Test fixtures, sandbox-only components, debug command components. |
| Temporary PC Shell | Transitional playable C++ shell used before final UI. | Runtime APIs and temporary presentation snapshots. | Becoming final UI truth, direct sandbox/test ownership. |
| Dev Tools | Editor/dev-only helpers used to inspect, seed, grant, auto-play, or accelerate local development. | Production runtime. | Being required by production runtime startup or final flows. |
| Debug Commands | Console/Exec commands for inspection, validation, grants, auto-advance, smoke/manual QA. | Production runtime if gated to editor/dev/non-shipping. | Always-on production shipping command surface. |
| Sandbox | Dev-only maps/widgets/components for controlled local experimentation. | Production runtime. | Default production entry flow unless explicitly configured as dev entry. |
| Automation Tests | C++ automation tests and smoke flows. | Production runtime and test-only helpers. | Runtime classes depending on test code. |
| Test Fixtures | Example/test assets, throwaway IDs, data audit fixtures. | Automation tests, commandlets, documentation. | Production catalogs, loot tables, shared pools, default maps. |
| Legacy/To Remove | Old maps, Blueprint refs, debug widgets/assets, legacy DataAssets kept only for migration or matrix comparisons. | None, or documented migration-only reads. | Default startup flow, final UI, production data pools. |
| Docs/Scripts | Audits, roadmaps, validation entrypoints, generated audit outputs. | Project files and commandlets. | Gameplay runtime dependencies. |

## 3. Inventaire par dossier/fichier

### Source runtime and UI

| Path | Current category | Target category | Action | Risk | Justification |
| --- | --- | --- | --- | --- | --- |
| `Source/GrandpaGambit/Public/Game/Modes/GambitGameMode.h` | Production Runtime with non-shipping debug/sandbox accessors | Production Runtime | Pass 1 done: forward declarations, plain C++ getters, and raw component pointers gated with `!UE_BUILD_SHIPPING` | Medium | Shipping `AGambitGameMode` no longer exposes debug/sandbox component getters or component storage. |
| `Source/GrandpaGambit/Private/Game/Modes/GambitGameMode.cpp` | Production Runtime with non-shipping debug/sandbox subobjects | Production Runtime | Pass 1 done: debug/sandbox includes and subobject creation gated with `!UE_BUILD_SHIPPING` | Medium | Shipping constructor creates only `UGambitRoundFlowComponent`; debug and sandbox components are not default subobjects. |
| `Source/GrandpaGambit/Public/Players/Controllers/GambitPlayerController.h` | Production Runtime plus gated Debug Commands | Production Runtime plus gated Debug Commands | Pass 1 done: exec declarations marked `meta=(DevelopmentOnly)`; debug/sandbox forward declarations and component helpers gated with `!UE_BUILD_SHIPPING` | Medium | UHT requires `UFUNCTION` declarations outside arbitrary preprocessor blocks, so the public exec surface is development-marked and runtime-gated. |
| `Source/GrandpaGambit/Private/Players/Controllers/GambitPlayerController.cpp` | Production Runtime plus gated Debug Commands | Production Runtime plus gated Debug Commands | Pass 1 done: debug/sandbox includes, real exec implementations, and helpers gated with `!UE_BUILD_SHIPPING`; Shipping stubs log unavailable commands | Medium | `GambitPrint...`, `GambitGrant...`, `GambitAuto...`, `GambitAI...`, and `GambitDev...` call debug/sandbox components only in non-shipping builds. |
| `Source/GrandpaGambit/Public/Game/States/GambitGameState.h` and `.cpp` | Production Runtime | Production Runtime | Keep | Low | Owns visible match state and runtime components; no test dependency found. |
| `Source/GrandpaGambit/Public/Game/Flow/GambitRoundFlowComponent.h` and `.cpp` | Production Runtime feedback | Production Runtime | Pass 2 done: score/gold/shop feedback uses stable breakdown names | Low | Generates `FGambitScoreBreakdownLine`, `FGambitGoldBreakdownLine`, and `FGambitShopBreakdownLine` without changing scoring/shop behavior. |
| `Source/GrandpaGambit/Public/Game/Flow/GambitRoundEffectPipeline.h` and `.cpp` | Production Runtime feedback | Production Runtime | Pass 2 done: appends stable feedback arrays | Low | Commits `RoundFeedbackEvents`, `ScoreBreakdownLines`, `GoldBreakdownLines`, and `ShopBreakdownLines`. |
| `Source/GrandpaGambit/Public/Game/Flow/GambitTargetSelectionService.h` and `.cpp` | Production Runtime presentation text | Production Runtime | Pass 2 done: `DebugText` renamed to `PresentationText` | Low | Text remains player-facing summary/invalid reason preview copy. |
| `Source/GrandpaGambit/Public/Core/Types/GambitRoundFeedbackTypes.h` | Runtime feedback and snapshots | Production Runtime feedback/presentation types | Pass 2 done: replaces `GambitDebugTypes.h` | Low | Owns stable feedback/snapshot structs for runtime and V0.1 shell consumption. |
| `Source/GrandpaGambit/Public/Core/Types/GambitTargetSelectionTypes.h` | Production Runtime presentation text | Production Runtime | Pass 2 done: `PresentationText` fields | Low | Field is used by PC shell and target service as presentation text. |
| `Source/GrandpaGambit/Public/Items/Effects/GambitEffectExecutionTypes.h` | Production Runtime feedback arrays | Production Runtime | Pass 2 done: feedback arrays renamed | Low | Effect execution context stores stable score/gold/shop/effect feedback. |
| `Source/GrandpaGambit/Public/Players/Components/GambitPlayerRoundStateComponent.h` and `.cpp` | Production Runtime feedback storage | Production Runtime | Pass 2 done: feedback storage/API renamed | Low | Stores round breakdowns for runtime display and audit with stable naming. |
| `Source/GrandpaGambit/Public/Players/States/GambitPlayerState.h` and `.cpp` | Production Runtime snapshot API | Production Runtime | Pass 2 done: snapshot and feedback APIs renamed | Low | Exposes `BuildPlayerSnapshot()`, `BuildDiceSnapshot()`, and `Get...BreakdownLines()` APIs. |
| `Source/GrandpaGambit/Public/Players/Components/GambitEconomyComponent.h` and `.cpp` | Production Runtime gold feedback | Production Runtime | Pass 2 done: gold feedback output renamed | Low | Gold breakdown lines remain production feedback. |
| `Source/GrandpaGambit/Public/UI/Widgets/GambitPCShellWidget.h` and `.cpp` | Temporary PC Shell / Production UI V0.1 | Temporary PC Shell until final UI replaces it | Pass 2 done for API names; keep for V0.1 | Medium | Enables current playable loop and now consumes stable feedback/snapshot APIs. |
| `Source/GrandpaGambit/Public/UI/ViewModels/GambitMatchViewModel.h` and `.cpp` | UI snapshot API | Production UI API | Pass 2 done: `BuildPlayerSnapshots()` returns `FGambitPlayerSnapshot` | Low | Future UI no longer needs `FGambitDebugPlayerSnapshot` as its main contract. |
| `Source/GrandpaGambit/Public/UI/Widgets/GambitDevMatchSandboxWidget.h` and `.cpp` | Dev Sandbox UI | Sandbox / Dev Tools | Pass 1 partial: `ResolveSandboxComponent()` returns `nullptr` in Shipping | Medium | Correct category; still a dev tool, and it no longer resolves a production `GameMode` sandbox component in Shipping. |

### Source debug, sandbox, tests, validation

| Path | Current category | Target category | Action | Risk | Justification |
| --- | --- | --- | --- | --- | --- |
| `Source/GrandpaGambit/Public/Debug/GambitMatchDebugComponent.h` and `.cpp` | Debug Commands / Dev Tools | Debug Commands gated to editor/dev/non-shipping | Pass 1 done for production ownership | Medium | Contains grants, auto-advance, auto full match, print/validation helpers; reachable from `AGambitGameMode`/`AGambitPlayerController` only under `!UE_BUILD_SHIPPING`. |
| `Source/GrandpaGambit/Public/Debug/GambitDevMatchSandboxComponent.h` and `.cpp` | Sandbox / Dev Tools | Sandbox / Dev Tools | Pass 1 done for production ownership | Medium | Useful dev component; no longer created by production `GameMode` in Shipping. |
| `Source/GrandpaGambit/Public/Debug/GambitDevMatchSandboxTypes.h` | Sandbox data types | Sandbox / Dev Tools | Pass 2 mechanical update only | Medium | Dev sandbox now embeds `FGambitPlayerSnapshot`; sandbox/debug ownership remains unchanged. |
| `Source/GrandpaGambit/Public/Debug/GambitDebugAutoPlayer.h` and `.cpp` | Debug AI / smoke helper | Dev Tools / Automation helper | Keep isolated from production flow | Medium | Useful for command-driven smoke/manual tests, not gameplay AI. |
| `Source/GrandpaGambit/Private/Tests/GambitEffectResolverTests.cpp` | Automation Tests | Automation Tests | Keep | Low | Guarded by `WITH_DEV_AUTOMATION_TESTS`; tests runtime effect logic. |
| `Source/GrandpaGambit/Private/Tests/GambitPCShellFlowTests.cpp` | Automation Tests / smoke flow | Automation Tests | Keep | Low | Guarded by `WITH_DEV_AUTOMATION_TESTS`; exercises V0.1 PC shell flow. |
| `Source/GrandpaGambit/Private/Tests/GambitTargetSelectionTests.cpp` | Automation Tests | Automation Tests | Keep | Low | Guarded by `WITH_DEV_AUTOMATION_TESTS`; verifies target selection. |
| `Source/GrandpaGambit/Public/Data/Validation/*Commandlet.h` and `Private/Data/Validation/*Commandlet.cpp` | Data validation commandlets | Docs/Scripts support / Dev validation | Keep | Low | Commandlets are invoked by validation scripts and do not represent runtime flow. |
| `Source/GrandpaGambit/GrandpaGambit.Build.cs` | Build configuration | Build configuration | Review in later pass only if needed | Low | No separate test module exists yet; current tests are dev-automation guarded. |

### Config

| Path | Current category | Target category | Action | Risk | Justification |
| --- | --- | --- | --- | --- | --- |
| `Config/DefaultEngine.ini` | Production config with dev asset reference | Production config | Replace or rename/move dev GameInstance later | High | `GameInstanceClass` points to `/Blueprints/Dev/BP_GambitGameInstance`. |
| `Config/DefaultEngine.ini` maps | Production config | Production config | Keep default map if editor cleanup confirms no legacy refs | Medium | `GameDefaultMap` and `EditorStartupMap` both point to `Gambit.umap`; docs report a legacy missing debug widget load warning. |
| `Config/DefaultGame.ini` | Production settings/data config | Production config | Keep | Low | Points to production data settings/catalog/loadout/shared pool/shop definitions. |
| `Config/DefaultInput.ini` | Production input config with console enabled | Production config plus dev/debug console policy | Gate/disable risky debug commands later, not necessarily the console itself | Medium | Console key is enabled, making exec command exposure important. |
| `Config/DefaultEditor.ini` | Editor config | Docs/Scripts / Editor config | Keep | Low | No production dependency concern found in this audit. |

### Scripts and docs

| Path | Current category | Target category | Action | Risk | Justification |
| --- | --- | --- | --- | --- | --- |
| `Scripts/Run-GambitValidation.ps1` | Canonical validation gate | Docs/Scripts | Keep | Low | Must remain the local validation entrypoint. |
| `Scripts/README.md` | Validation documentation | Docs/Scripts | Keep | Low | Documents build, data audit, automation, matrix comparison, diff checks. |
| `Docs/Audits/PostFoundationV01Roadmap.md` | Roadmap/audit | Docs/Scripts | Keep | Low | Defines V0.1 backlog and constraints. |
| `Docs/Audits/V01PCPlayableLoopAudit.md` | Playable loop audit | Docs/Scripts | Keep | Low | Documents current playable shell and known legacy map warning. |
| `Docs/V01PCShell.md` | PC shell doc | Docs/Scripts | Keep, update after PC shell pass | Low | Correctly states PC shell is minimal and not final UI. |
| `Docs/V01RoundHUD.md` | Round HUD/shell doc | Docs/Scripts | Keep, update after UI API pass | Low | Clarifies current text-first shell behavior. |
| `Docs/Audits/DataAssetsAudit.md` | Generated/manual audit output | Docs/Scripts | Keep | Low | Tracks data audit and exclusions. |
| `Docs/Audits/DataAssetsAuditExclusions.json` | Audit fixture/exclusion policy | Docs/Scripts / Test Fixtures policy | Keep | Low | Excludes examples/legacy/smoke assets from matrix comparison. |
| `Docs/RoundGameplayEvents.md` | Runtime feedback documentation | Docs/Scripts | Keep, align names after feedback rename | Low | Already explains that event ledger is feedback/debug audit, not authoritative replay. |

### Content inventory

Binary `.uasset` files were inventoried by path only. They were not edited.

| Path | Current category | Target category | Action | Risk | Justification |
| --- | --- | --- | --- | --- | --- |
| `Content/GrandpaGambit/Maps/Gambit.umap` | Current default map | Production entry map | Inspect in editor later | Medium | Default map; docs report legacy `WBP_DebugMatchInspector` load issue during automation startup. |
| `Content/GrandpaGambit/Maps/L_DevMatchSandbox.umap` | Sandbox map | Sandbox | Keep isolated from default startup | Medium | Dev sandbox should not become normal player flow. |
| `Content/GrandpaGambit/Maps/L_Menu_DevMatch.umap` | Dev menu/legacy map | Sandbox / Legacy | Keep until replacement, then remove later | Medium | Not default entry per docs; should stay dev-only. |
| `Content/GrandpaGambit/Blueprints/Dev/BP_GambitGameInstance.uasset` | Dev Blueprint used by default config | Production config issue or mislocated prod asset | Replace, rename, or move later | High | DefaultEngine points production startup to a `Dev` path. |
| `Content/GrandpaGambit/Blueprints/Game/BP_GambitGameMode.uasset` | Production Blueprint | Production Runtime integration | Inspect only if GameMode separation changes C++ defaults | Medium | Blueprint derives from production mode; may inherit debug/sandbox component subobjects. |
| `Content/GrandpaGambit/Blueprints/Game/BP_GambitGameState.uasset` | Production Blueprint | Production Runtime integration | Keep | Low | No direct issue found by path. |
| `Content/GrandpaGambit/Blueprints/Players/BP_GambitPlayerController.uasset` | Production Blueprint | Production Runtime integration | Inspect after controller command gating | Medium | Controller class currently exposes debug/dev exec commands. |
| `Content/GrandpaGambit/Blueprints/Players/BP_GambitPlayerState.uasset` | Production Blueprint | Production Runtime integration | Keep | Low | No direct issue found by path. |
| `Content/GrandpaGambit/Blueprints/UI/Debug/WBP_Dev*.uasset` | Dev/debug UI widgets | Dev Tools / Sandbox UI | Keep isolated, remove after replacement only | Medium | Debug UI assets should not be part of final player flow. |
| `Content/GrandpaGambit/Data/Examples/Validation/*.uasset` | Test fixtures / validation examples | Test Fixtures | Keep excluded from production pools | Low | Expected fixtures referenced by data audit exclusions. |
| `Content/GrandpaGambit/Data/Items/Effects/**Legacy**.uasset` | Legacy compatibility/smoke assets | Legacy/To Remove after replacement | Keep until migration policy says otherwise | Medium | Excluded or legacy-tagged assets should not be in production offer/shared pools unless intentionally migrated. |
| `Content/GrandpaGambit/Data/Dice`, `Data/Items`, `Data/Shop` | Production data | Production Runtime data | Keep | Low | Default settings point here for real catalogs/loadouts/pools. |

## 4. Dependances problematiques

| Caller | Callee | Dependency type | Why problematic | Recommended solution | Priority |
| --- | --- | --- | --- | --- | --- |
| `AGambitGameMode` | `UGambitMatchDebugComponent` | Non-shipping compile-time include, public getter, default subobject ownership | Fixed for Shipping in Pass 1. | `#if !UE_BUILD_SHIPPING` now gates the include, getter, property, and default subobject creation. | P0 done |
| `AGambitGameMode` | `UGambitDevMatchSandboxComponent` | Non-shipping compile-time include, public getter, default subobject ownership | Fixed for Shipping in Pass 1. | `#if !UE_BUILD_SHIPPING` now gates the include, getter, property, and default subobject creation. | P0 done |
| `AGambitPlayerController` | `UGambitMatchDebugComponent` | Development-marked exec commands call debug component only in non-shipping implementations | Fixed for Shipping in Pass 1. | `#if !UE_BUILD_SHIPPING` now gates debug includes, real command implementations, and helper resolution; Shipping stubs do not touch debug components. | P0 done |
| `AGambitPlayerController` | `UGambitDevMatchSandboxComponent` | Development-marked exec commands call sandbox component only in non-shipping implementations | Fixed for Shipping in Pass 1. | `#if !UE_BUILD_SHIPPING` now gates sandbox includes, real command implementations, and helper resolution; Shipping stubs do not touch sandbox components. | P0 done |
| `AGambitPlayerController` | `UGambitPCShellWidget` | Runtime widget bootstrap | Acceptable for V0.1, but blocks final UI if treated as final. | Keep for V0.1; introduce a final UI bootstrap/presentation policy later. | P2 |
| Runtime feedback systems | `Core/Types/GambitRoundFeedbackTypes.h` | Runtime state and execution contexts use stable feedback/snapshot types | Fixed in Pass 2. | `GambitDebugTypes.h` was replaced by `GambitRoundFeedbackTypes.h`; runtime systems use round feedback, score/gold/shop breakdown, and snapshot names. | P1 done |
| `UGambitMatchViewModel` | `FGambitPlayerSnapshot` | UI-facing API returns stable player snapshot types | Fixed in Pass 2. | `BuildDebugPlayerSnapshots()` was replaced with `BuildPlayerSnapshots()`. | P1 done |
| `FGambitTargetSelectionRequest/Option` | `PresentationText` | Runtime target selection API exposes production presentation copy | Fixed in Pass 2. | `DebugText` was renamed to `PresentationText` without changing target selection behavior. | P2 done |
| `Config/DefaultEngine.ini` | `/Blueprints/Dev/BP_GambitGameInstance` | Production startup config points to Dev asset | Default game boot path depends on an asset categorized as dev. | Replace with production GameInstance asset/class, or rename/move the asset if it is actually production. | P1 |
| `Gambit.umap` content path | Missing/legacy `WBP_DebugMatchInspector` reference reported by existing audit | Binary Blueprint/map reference | Startup automation logs a legacy debug widget dependency. | Open in editor, remove stale widget reference, resave map/Blueprint in a content cleanup pass. | P1 |
| `Config/DefaultInput.ini` console key + controller exec commands | Debug/cheat command surface | Runtime input can reach debug commands in normal builds | Console itself can be useful, but debug/grant commands must be gated. | Gate commands first; then decide shipping console policy. | P1 |

## 5. Dependances acceptables

These dependency directions are acceptable and should remain allowed:

- Automation tests -> production runtime.
- Smoke flows -> production runtime, when guarded by automation/dev build flags.
- Sandbox maps/widgets/components -> production runtime.
- Debug commands -> production runtime, only when editor/dev/non-shipping gated.
- Data validation commandlets -> production DataAssets and runtime definitions.
- Scripts/docs -> commandlets/build/automation outputs.
- Production UI -> stable production runtime APIs, GameState/PlayerState snapshots, and `Request...` command entry points.

These directions must remain forbidden:

- Production runtime -> automation tests.
- Production runtime -> test fixtures.
- Production runtime -> sandbox maps/widgets/components.
- Production runtime -> debug command components.
- Production UI final -> debug snapshots or sandbox-only APIs.

## 6. Liste des elements a nettoyer

### A. Nettoyage immediat safe

- Add this audit document.
- Do not move/delete assets in this pass.
- Do not rename code in this pass.
- Record known boundaries and use this document as the execution checklist.

### B. Isolation necessaire avant UI finale

- [x] Isolate `UGambitMatchDebugComponent` and `UGambitDevMatchSandboxComponent` from `AGambitGameMode` production Shipping ownership.
- [x] Gate `AGambitPlayerController` debug/dev `Exec` command behavior with `meta=(DevelopmentOnly)`, real implementations under `!UE_BUILD_SHIPPING`, and Shipping stubs.
- [x] Replace debug-named runtime feedback types with stable production feedback/presentation names.
- [x] Replace `UGambitMatchViewModel::BuildDebugPlayerSnapshots()` with stable UI snapshots.
- [x] Rename target selection `DebugText` fields to production presentation names.
- Replace or relocate `/Blueprints/Dev/BP_GambitGameInstance` if it is part of normal startup.
- Clean stale legacy debug widget references from `Gambit.umap` or the Blueprint that owns them.

### C. Suppression a faire seulement apres remplacement

- Old debug/sandbox maps, if no longer used after final menu/setup flow exists.
- `WBP_Dev*` widgets after their dev utility is either replaced or explicitly preserved elsewhere.
- Legacy/smoke DataAssets after matrix/data audit confirms they are no longer needed.
- Old aliases for runtime feedback types, if a compatibility migration is introduced later.

### D. A garder mais renommer/documenter

- `UGambitPCShellWidget`: keep for V0.1, consider `UGambitV01PCShellWidget` later only if rename cost is acceptable.
- Runtime feedback structs: behavior kept; Pass 2 renamed them to stable production feedback/snapshot names.
- Data validation commandlets: keep as validation tooling.
- `UGambitDebugAutoPlayer`: keep as debug/smoke helper, not production AI.
- `Scripts/Run-GambitValidation.ps1`: keep as canonical gate.

### E. A ne pas toucher

- Production data catalogs/loadouts/shop/shared pool assets unless a data audit specifically fails.
- Automation test intent and coverage.
- `Scripts/Run-GambitValidation.ps1` unless adding/renaming validation suites or generated audit outputs.
- `Scripts/README.md` unless validation behavior changes.
- Final UI implementation is out of scope for this separation audit.

## 7. Decision sur le shell PC actuel

Decision: `UGambitPCShellWidget` is a **Temporary PC Shell that also serves as the Production UI V0.1**.

Answers:

- Is it a temporary tool or V0.1 base?
  It is the V0.1 playable shell and a temporary bridge, not the final UI architecture.

- Can it stay in runtime for V0.1?
  Yes. The current target is PC keyboard/mouse, and the shell provides the playable loop without owning gameplay truth.

- Should it be renamed to `DevShell`, `PCShell`, or another name?
  Do **not** rename it to `DevShell`; that would incorrectly classify the V0.1 player path as dev-only. `GambitPCShellWidget` is acceptable for now. `GambitV01PCShellWidget` would be clearer later, but only in a dedicated rename pass.

- What should final UI replace or reuse?
  Final UI should replace the shell presentation and consume stable runtime APIs: GameState/PlayerState snapshots, `Request...` commands, RoundFlow status, shop/inventory/dice/target selection snapshots. It may reuse small formatting concepts, but not the shell layout or debug-named snapshot contract as the final foundation.

## 8. Plan d'execution en passes

### Pass 1 - Runtime gate for debug/sandbox ownership

Objective: ensure production runtime no longer unconditionally owns or exposes debug/sandbox components.

Probable files:

- `Source/GrandpaGambit/Public/Game/Modes/GambitGameMode.h`
- `Source/GrandpaGambit/Private/Game/Modes/GambitGameMode.cpp`
- `Source/GrandpaGambit/Public/Players/Controllers/GambitPlayerController.h`
- `Source/GrandpaGambit/Private/Players/Controllers/GambitPlayerController.cpp`
- `Source/GrandpaGambit/Public/Debug/*`
- `Source/GrandpaGambit/Private/Debug/*`
- Possibly affected automation tests that call exec commands.

Allowed actions:

- Add editor/dev/non-shipping guards or a narrow dev-only access layer.
- Keep commands available for local dev and automation where needed.
- Keep V0.1 PC shell behavior unchanged.
- Update this audit with exact implemented boundary.

Forbidden actions:

- No final UI implementation.
- No map/DataAsset creation.
- No gameplay rule changes.
- No deletion of debug/sandbox tools.

Expected validation:

- Full `Scripts/Run-GambitValidation.ps1`.
- Confirm automation smoke tests still run.
- Confirm production runtime can start without requiring sandbox/debug components.

Risk: High, because command gating can break smoke automation if done carelessly.

Done criteria:

- `AGambitGameMode` no longer unconditionally creates debug/sandbox components for production runtime.
- `AGambitPlayerController` debug/sandbox exec commands are editor/dev/non-shipping gated or moved.
- Tests and validation pass.

Pass 1 implemented boundary:

- `Source/GrandpaGambit/Public/Game/Modes/GambitGameMode.h`: debug/sandbox forward declarations, plain C++ `GetMatchDebugComponent()`, `GetDevMatchSandboxComponent()`, `MatchDebugComponent`, and `DevMatchSandboxComponent` are wrapped in `#if !UE_BUILD_SHIPPING`; they are not UHT-reflected in Shipping.
- `Source/GrandpaGambit/Private/Game/Modes/GambitGameMode.cpp`: debug/sandbox includes and `CreateDefaultSubobject` calls are wrapped in `#if !UE_BUILD_SHIPPING`; Shipping creates only the production `RoundFlowComponent` from this group.
- `Source/GrandpaGambit/Public/Players/Controllers/GambitPlayerController.h`: all debug/dev `UFUNCTION(Exec)` declarations are marked `meta=(DevelopmentOnly)`; debug/sandbox forward declarations and private component helper declarations are wrapped in `#if !UE_BUILD_SHIPPING`.
- `Source/GrandpaGambit/Private/Players/Controllers/GambitPlayerController.cpp`: debug/sandbox includes, all real `Gambit...` debug/dev exec implementations, and the private debug/sandbox helper definitions are wrapped in `#if !UE_BUILD_SHIPPING`; Shipping builds compile command stubs that only log unavailability.
- `Source/GrandpaGambit/Private/UI/Widgets/GambitDevMatchSandboxWidget.cpp`: `ResolveSandboxComponent()` resolves through `AGambitGameMode` only under `#if !UE_BUILD_SHIPPING`; Shipping returns `nullptr`.

Commands still available with real behavior under `!UE_BUILD_SHIPPING`; in Shipping they are `DevelopmentOnly` exec stubs that log unavailability and do not touch debug/sandbox components:

- Debug inspection/validation: `Gambit`, `GambitPrintMatch`, `GambitValidateData`, `GambitPrintInventory`, `GambitPrintSharedPool`, `GambitPrintShop`.
- Debug phase/shop helpers: `GambitReadyAll`, `GambitAutoAdvance`, `GambitAutoToShop`, `GambitBuyFirstOfferAll`, `GambitSkipShop`, `GambitBuyOffer`.
- Debug grants/player actions: `GambitGrantGold`, `GambitGrantConsumable`, `GambitRerollPlayer`, `GambitLockDie`, `GambitUseConsumable`, `GambitUseConsumableOnDie`.
- Debug AI/smoke helpers: `GambitAutoFullMatch`, `GambitAIDecideRerolls`, `GambitAIDecideActions`, `GambitAIDecideShop`, `GambitAIFullRound`, `GambitAIFullMatch`.
- Dev sandbox commands: `GambitDevStart`, `GambitDevSnapshot`, `GambitDevInspect`, `GambitDevHuman`, `GambitDevAI`, `GambitDevAdvance`, `GambitDevAIDecide`.

### Pass 2 - Rename runtime feedback APIs away from `Debug`

Objective: convert runtime feedback/breakdown/snapshot semantics from debug naming to production naming.

Probable files:

- `Source/GrandpaGambit/Public/Core/Types/GambitDebugTypes.h`
- `Source/GrandpaGambit/Public/Players/Components/GambitPlayerRoundStateComponent.h`
- `Source/GrandpaGambit/Public/Players/States/GambitPlayerState.h`
- `Source/GrandpaGambit/Public/Items/Effects/GambitEffectExecutionTypes.h`
- Runtime `.cpp` files that add score/gold/shop/effect lines.
- Tests and PC shell compile references.

Allowed actions:

- Introduce stable names such as `GambitRoundFeedbackTypes`, `FGambitScoreBreakdownLine`, `FGambitPlayerRuntimeSnapshot`.
- Use compatibility aliases only if needed to keep pass size controlled.
- Update tests to assert feedback behavior under new names.

Forbidden actions:

- No behavior change to scoring/economy/effects.
- No final UI design work.

Expected validation:

- Full validation gate.
- Additional compile grep for old `FGambitDebug...` runtime references if feasible.

Risk: Medium-high due widespread references.

Done criteria:

- Runtime production systems no longer expose debug-named feedback as their primary API.

Pass 2 implemented boundary:

- `Source/GrandpaGambit/Public/Core/Types/GambitDebugTypes.h` was renamed to `Source/GrandpaGambit/Public/Core/Types/GambitRoundFeedbackTypes.h`; no compatibility alias was kept because source references were migrated directly.
- `FGambitDebugEffectEvent` -> `FGambitRoundFeedbackEvent`.
- `EGambitDebugEventCategory` -> `EGambitRoundFeedbackEventCategory`.
- `FGambitDebugScoreLine` -> `FGambitScoreBreakdownLine`.
- `EGambitDebugScoreLineType` -> `EGambitScoreBreakdownLineType`.
- `FGambitDebugGoldLine` -> `FGambitGoldBreakdownLine`.
- `EGambitDebugGoldLineType` -> `EGambitGoldBreakdownLineType`.
- `FGambitDebugShopLine` -> `FGambitShopBreakdownLine`.
- `EGambitDebugShopLineType` -> `EGambitShopBreakdownLineType`.
- `FGambitDebugDieSnapshot` -> `FGambitDiceSnapshot`.
- `FGambitDebugItemSnapshot` -> `FGambitItemSnapshot`.
- `FGambitDebugPlayerSnapshot` -> `FGambitPlayerSnapshot`.
- `AddDebugEffectEvent()` -> `AddRoundFeedbackEvent()`.
- `AddDebugScoreLine()` -> `AddScoreBreakdownLine()`.
- `AddDebugGoldLine()` -> `AddGoldBreakdownLine()`.
- `AddDebugShopLine()` -> `AddShopBreakdownLine()`.
- `AppendDebugEffectEvents()` -> `AppendRoundFeedbackEvents()`.
- `AppendDebugScoreLines()` -> `AppendScoreBreakdownLines()`.
- `AppendDebugGoldLines()` -> `AppendGoldBreakdownLines()`.
- `AppendDebugShopLines()` -> `AppendShopBreakdownLines()`.
- `GetDebugEffectEvents()` -> `GetRoundFeedbackEvents()`.
- `GetDebugScoreLines()` -> `GetScoreBreakdownLines()`.
- `GetDebugGoldLines()` -> `GetGoldBreakdownLines()`.
- `GetDebugShopLines()` -> `GetShopBreakdownLines()`.
- `BuildDebugDiceSnapshot()` -> `BuildDiceSnapshot()`.
- `BuildDebugModuleSnapshot()` -> `BuildModuleSnapshot()`.
- `BuildDebugConsumableSnapshot()` -> `BuildConsumableSnapshot()`.
- `BuildDebugPlayerSnapshot()` -> `BuildPlayerSnapshot()`.
- `BuildDebugPlayerSnapshots()` -> `BuildPlayerSnapshots()`.
- `BuildDebugPlayerSnapshotByIndex()` -> `BuildPlayerSnapshotByIndex()`.
- `DebugEffectEvents` -> `RoundFeedbackEvents`.
- `DebugScoreLines` -> `ScoreBreakdownLines`.
- `DebugGoldLines` -> `GoldBreakdownLines`.
- `DebugShopLines` -> `ShopBreakdownLines`.
- `NextDebugSequence` -> `NextFeedbackSequence`.
- `DebugText` -> `PresentationText`.
- Runtime `DebugLines` shop feedback -> `PresentationLines`.

Files updated for Pass 2:

- Runtime feedback/type contracts: `Core/Types/GambitRoundFeedbackTypes.h`, `Core/Types/GambitGameplayTypes.h`, `Core/Types/GambitTargetSelectionTypes.h`, `Items/Effects/GambitEffectExecutionTypes.h`.
- Runtime owners and flow: `Players/Components/GambitPlayerRoundStateComponent.*`, `Players/States/GambitPlayerState.*`, `Players/Components/GambitEconomyComponent.*`, `Game/Flow/GambitRoundFlowComponent.cpp`, `Game/Flow/GambitRoundEffectPipeline.cpp`, `Game/Flow/GambitTargetSelectionService.cpp`, `Items/Effects/GambitEffectResolver.cpp`, `Shop/Components/GambitShopComponent.cpp`, `Players/Controllers/GambitPlayerController.cpp`.
- V0.1 shell and viewmodel compile migration: `UI/Widgets/GambitPCShellWidget.cpp`, `UI/ViewModels/GambitMatchViewModel.*`.
- Dev/debug compile consumers only: `Debug/GambitMatchDebugComponent.cpp`, `Debug/GambitDevMatchSandboxComponent.*`, `Debug/GambitDevMatchSandboxTypes.h`, `Debug/GambitDebugAutoPlayer.cpp`.
- Automation/smoke compile migration only: `Private/Tests/GambitEffectResolverTests.cpp`, `Private/Tests/GambitPCShellFlowTests.cpp`, `Private/Tests/GambitTargetSelectionTests.cpp`.

Pass 2 explicit non-changes:

- No gameplay rules, scoring math, economy math, shop logic, dice rules, target selection rules, final UI layout, DataAssets, maps, Blueprints, validation scripts, or debug/sandbox ownership rules were changed.
- `Source/Debug` tools and sandbox components remain available under the Pass 1 non-shipping boundary.
- V0.1 PC shell behavior remains the same; only the names of consumed feedback/snapshot APIs changed.

### Pass 3 - Stable production UI snapshot and command contract

Objective: define the runtime API final UI will consume.

Probable files:

- `Source/GrandpaGambit/Public/UI/ViewModels/GambitMatchViewModel.h`
- `Source/GrandpaGambit/Private/UI/ViewModels/GambitMatchViewModel.cpp`
- `Source/GrandpaGambit/Public/Players/States/GambitPlayerState.h`
- `Source/GrandpaGambit/Public/Game/States/GambitGameState.h`
- Target selection, shop, inventory, dice state structs.

Allowed actions:

- Add stable UI snapshots/viewmodels.
- Route `Request...` commands through production controller/GameMode/RoundFlow APIs.
- Keep PC shell on old API until migration is intentional, or migrate it mechanically.

Forbidden actions:

- No final layout/widgets.
- No gamepad navigation.
- No multi-target UI beyond existing PC V0.1 scope.

Expected validation:

- Full gate plus targeted compile/tests.

Risk: Medium.

Done criteria:

- Final UI can be started later without depending on debug/sandbox/test APIs.

### Pass 4 - Content/config default-flow cleanup

Objective: remove default production startup references to dev/legacy content.

Probable files/assets:

- `Config/DefaultEngine.ini`
- `Content/GrandpaGambit/Blueprints/Dev/BP_GambitGameInstance.uasset`
- `Content/GrandpaGambit/Maps/Gambit.umap`
- Any Blueprint that still references `WBP_DebugMatchInspector`.

Allowed actions:

- Rename/move/repoint GameInstance if it is actually production.
- Open editor to remove stale widget references and resave assets.
- Keep dev maps as dev maps.

Forbidden actions:

- No broad content deletion.
- No gameplay/UI redesign.

Expected validation:

- Full gate.
- Verify startup log no longer reports missing `WBP_DebugMatchInspector`.

Risk: Medium, because binary asset edits require editor validation.

Done criteria:

- Default config no longer points to a `Dev` asset for normal startup.
- Default map has no stale debug widget load errors.

### Pass 5 - PC shell quarantine/deprecation policy

Objective: explicitly freeze the PC shell as V0.1 temporary UI and prevent it from becoming final UI architecture by accident.

Probable files:

- `Source/GrandpaGambit/Public/UI/Widgets/GambitPCShellWidget.h`
- `Source/GrandpaGambit/Private/UI/Widgets/GambitPCShellWidget.cpp`
- `Docs/V01PCShell.md`
- `Docs/V01RoundHUD.md`

Allowed actions:

- Rename to `GambitV01PCShellWidget` only if final UI API is already stable.
- Add doc comments or deprecation notes.
- Keep V0.1 playability intact.

Forbidden actions:

- No final UI implementation in this pass.
- No removal before replacement.

Expected validation:

- Full gate, especially PC shell smoke flow.

Risk: Medium.

Done criteria:

- Shell role is clear in code/docs and cannot be mistaken for final UI.

### Pass 6 - Legacy/test fixture asset policy

Objective: confirm fixture/legacy/smoke DataAssets are excluded from production runtime pools.

Probable files:

- `Docs/Audits/DataAssetsAudit.md`
- `Docs/Audits/DataAssetsAuditExclusions.json`
- Production catalogs/loot/shared pool assets if audit reveals contamination.

Allowed actions:

- Update exclusions and docs if policy changes.
- Remove production references to fixtures if found.

Forbidden actions:

- No deleting fixture assets without a replacement/migration note.

Expected validation:

- DataAssets audit and matrix comparison pass.

Risk: Low-medium.

Done criteria:

- Production pools do not depend on fixtures/legacy/smoke assets unless explicitly accepted.

### Pass 7 - Validation/script tightening

Objective: add automated checks for the separation boundary if needed.

Probable files:

- `Scripts/Run-GambitValidation.ps1`
- `Scripts/README.md`
- Optional generated audit/check output docs.

Allowed actions:

- Add a narrow grep/static check for forbidden production -> debug/sandbox/test references.
- Document the new check.

Forbidden actions:

- No broad script rewrite.
- No replacing the canonical validation entrypoint.

Expected validation:

- The validation script validates itself successfully.

Risk: Low.

Done criteria:

- Boundary regressions are detectable in the normal local gate.

## 9. Prompt suivant

Use this prompt only if the Pass 2 validation gate passes. If the gate fails, run a minimal correction pass for the validation failure first.

```text
Tu travailles sur Grandpa Gambit, projet Unreal Engine 5 C++.

Objectif de cette passe:
Executer uniquement la Pass 3 de `Docs/Audits/ProdDevTestSeparationAudit.md`: definir le contrat stable de snapshots/commandes que la future UI finale consommera.

Contraintes:
- Ne pas faire la Pass 4+.
- Ne pas faire UI finale/layout final.
- Ne pas ajouter de navigation gamepad/manette.
- Ne pas creer de nouveaux DataAssets ou maps.
- Ne pas supprimer de fichiers/assets.
- Ne pas modifier les regles de gameplay, scoring, economie, shop, dice, target selection.
- Garder le shell PC V0.1 jouable.
- Garder les tests automation/smoke utilisables.

Travail attendu:
1. Relire `AGENTS.md` et `Docs/Audits/ProdDevTestSeparationAudit.md`.
2. Auditer les API UI/runtime existantes de `UGambitMatchViewModel`, `AGambitPlayerState`, `AGambitGameState`, shop/inventory/dice/target selection.
3. Ajouter ou consolider uniquement les snapshots et `Request...` APIs stables necessaires a une future UI finale.
4. Garder le PC shell sur un contrat compile et jouable.
5. Mettre a jour `Docs/Audits/ProdDevTestSeparationAudit.md` avec le resultat exact de la Pass 3.
6. Lancer:
   `powershell -ExecutionPolicy Bypass -File .\Scripts\Run-GambitValidation.ps1`

Sortie attendue:
- fichiers modifies;
- resume technique du contrat UI/runtime;
- confirmation qu'aucun comportement gameplay/UI finale n'a change;
- resultat complet du gate.
```

## 10. Taches restantes apres la Pass 2

- [x] Pass 2 - Rename runtime feedback APIs away from `Debug`.
- [ ] Pass 3 - Stable production UI snapshot and command contract.
- [ ] Pass 4 - Content/config default-flow cleanup.
- [ ] Pass 5 - PC shell quarantine/deprecation policy.
- [ ] Pass 6 - Legacy/test fixture asset policy.
- [ ] Pass 7 - Validation/script tightening.

## 11. Validation locale de la Pass 1

Status: **passed** on 2026-06-30.

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Run-GambitValidation.ps1
```

Result:

| Gate item | Result | Details |
| --- | --- | --- |
| Build | OK | `GrandpaGambitEditor Win64 Development` built successfully. |
| DataAssets audit | OK | `total=291`, `valid=291`, `warning=0`, `error=0`. |
| Audit files / matrix comparison | OK | Audit generated outputs and comparison gate passed. |
| Automation | OK | `success=83`, `warning=0`, `failed=0`, `notRun=0`, `inProcess=0`. |
| `git diff --check` | OK | No whitespace/error findings. |

Logs:

```text
Saved/Automation/GambitValidation/20260630-094838
```

## 12. Validation locale de la Pass 2

Status: **passed** on 2026-06-30.

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Run-GambitValidation.ps1
```

Result:

| Gate item | Result | Details |
| --- | --- | --- |
| Build | OK | `GrandpaGambitEditor Win64 Development` built successfully. |
| DataAssets audit | OK | `total=291`, `valid=291`, `warning=0`, `error=0`. |
| Audit files / matrix comparison | OK | Audit generated outputs and comparison gate passed. |
| Automation | OK | `success=83`, `warning=0`, `failed=0`, `notRun=0`, `inProcess=0`. |
| `git diff --check` | OK | No whitespace/error findings. |

Logs:

```text
Saved/Automation/GambitValidation/20260630-105211
```
