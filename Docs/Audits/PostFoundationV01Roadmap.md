# Post-foundation / B6 -> V0.1 Roadmap

Date: 2026-06-28

Scope: audit and roadmap only. No B6 DataAssets, gameplay code, ObjectMatrix rows, UI systems, or gamepad/controller systems were created in this pass.

## Current status

Global status: **Ready with caveats** for a restricted B6.1 content wave.

The project foundations are in place for:

- typed negative-effect defenses;
- target rules v2 and target selection flow;
- ledger-based round feedback;
- round effect pipeline and resolver split by gameplay domain;
- configurable scoring;
- DataAsset audit with matrix exclusions;
- round gameplay event ledger;
- runtime dice state and runtime face overrides;
- runtime inventory item instances;
- canonical local validation through `Scripts/Run-GambitValidation.ps1`.

The project is not ready to blindly create all B6 rows. B6 rows that require true multi-target mutation, persistent per-player target state, previous-round snapshots, win streaks, random removal, partial mitigation, rare-only defense filtering, robust random effect pools, persistent copy/mutation, or advanced shop/table systems remain out of the first V0.1 content scope.

V0.1 target platform is PC. The current UI debt is final PC keyboard/mouse UI: focus, click, hover, and readable target-selection presentation. Gamepad/controller navigation is future work and is outside V0.1 scope.

## Sources reviewed

- `Docs/Audits/B6ReadinessAudit.md`
- `Docs/ObjectCreation/CreationBatches.md`
- `Docs/ObjectCreation/EffectsCoverage.md`
- `Docs/ObjectCreation/ObjectMatrix.csv`
- `Docs/TargetSelectionFlow.md`
- `Docs/RoundGameplayEvents.md`
- `Docs/RuntimeDiceState.md`
- `Docs/InventoryRuntimeInstances.md`
- `Docs/Audits/DataAssetsAudit.md`
- `Scripts/README.md`

## Validation run

Command run from repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Run-GambitValidation.ps1
```

Result: **pass**

| Gate | Result |
| --- | --- |
| Build `GrandpaGambitEditor Win64 Development` | OK |
| `GambitGameplayDataAudit` | OK |
| Audit JSON/CSV outputs | OK |
| DataAsset validation | 274 valid, 0 warnings, 0 errors |
| Automation `GrandpaGambit` | 67 success, 0 warnings, 0 failed, 0 notRun, 0 inProcess |
| Matrix comparison | OK |
| `git diff --check` | OK |

Run logs:

```text
Saved/Automation/GambitValidation/20260628-083333/
```

Audit output files:

```text
Saved/Automation/GambitDataAssetsAudit.json
Saved/Automation/GambitDataAssetsAudit.csv
```

Matrix comparison summary:

| Check | Result |
| --- | ---: |
| Matrix rows read/compared | 274 |
| Matched real assets | 135 |
| Planned missing rows | 139 |
| Actual assets missing from matrix | 0 |
| Excluded actual assets | 10 |
| Remaining actual assets missing from matrix | 0 |
| Type mismatches | 0 |
| Matrix status mismatches | 0 |
| Duplicate actual IDs | 0 |

The 139 planned missing rows are backlog/planning rows, including B6 and later objects. They are not validation failures.

## B6 remaining work

### A. B6 creatable now

These objects can be created without new gameplay systems if they are authored in the constrained B6.1 form described below:

- `module.defense.shield`: full `PreventNegativeEffect`, global or category-based, optionally one block if `PreventNegativeEffectBlockCount` is tested.
- `module.defense.safe`: full prevention for `GoldSteal` and/or `GoldLoss`.
- `module.defense.stable_table`: full prevention for `LockModification` and/or `ForcedReroll`; do not author locked-die-only filtering yet.
- `module.defense.rare_protection`: only if re-scoped from rare-only to full `DieDestroyOrRemove` prevention.
- `module.competitive.composure`: only if re-scoped from 50 percent mitigation to full category prevention.
- `module.defense.calm`: only if re-scoped from partial reduction to full `DieValueReduction` prevention.
- `consumable.tax_audit`: fixed amount, single target, no percent-of-current-gold logic.
- `module.competitive.luxury_tax`: fixed amount, single resolved target, with explicit source-is-target behavior tested.

Allowed patterns for this wave:

- typed full prevention;
- single-target fixed gold loss/steal or score penalty;
- player or die target rules already covered by the target selection foundation;
- ledger feedback used for visibility only, not as a complex authored condition;
- simple negative categories with a clear defense path.

### B. B6 needing a mini-system before creation

These are not recommended for B6.1, but could become B6.2 after a small, explicit mechanic is implemented and tested:

- `dice.vampire` and `item.dice.vampire`: real all-opponents score steal or a supported single-target redesign.
- `module.position.lateral_pressure`: true two-target execution for left/right players plus pair count.
- `module.legendary.cursed_table`: true `all_opponents` score mutation.
- `module.competitive.point_thief`: clear score-steal semantics with source/target score deltas and leader timing.
- `module.competitive.light_saboteur`: production PC target flow for opponent die selection if kept as a passive module.
- `module.competitive.threatening_look`: opponent reroll-count comparison.
- `module.competitive.vengeance`: previous-round rank snapshot.
- `module.mult.medium_bet`: delayed next-round buff storage.
- `module.competitive.insurance`: ledger/history condition for stolen gold and amount.
- `module.defense.anti_theft`: ledger/history condition for "was stolen from".
- `module.defense.score_insurance`: dynamic score floor.
- `module.rarity.monopoly`: opponent inventory comparison by rare dice ownership.
- `module.shared.disputed_collection`: opponent inventory comparison by shared rare die type.
- `consumable.risky_contract`: ranking-time random module removal.
- `module.risk.cursed_pact`: ranking-time random die removal.
- `module.risk.blood_for_gold`: acquisition timing or redesign as a consumable-like self trade.
- Original `module.competitive.composure` and `module.defense.calm`: partial mitigation.
- Original `module.defense.rare_protection`: rare-only defense filtering.

Mini-systems should stay narrow and data-driven. They should not become hidden B7/B8/B9 infrastructure.

### C. B6 to postpone

These should not be created now:

- `module.position.intimidation`: real table-position meaning belongs with the later table/seat work.
- `module.competitive.rivalry`: persistent rival selection.
- `module.competitive.jealous_copycat`: opponent score-breakdown copy semantics.
- `module.competitive.last_word`: turn order in a currently phase/simultaneous flow.
- `module.competitive.first_blood`: first-player and first-6 tracking.
- `module.competitive.table_king`: previous winner / win-streak state.
- `module.competitive.anti_leader`: previous winner plus unclear duel context.
- `module.risk.roulette`: robust random effect pool.
- `module.defense.immunity`: once-per-match loss override and unclear effect definition.
- `module.defense.false_weak`: mid-round rank snapshot.
- `module.legendary.final_boss`: circular rank-before-score logic and legendary/B9-style complexity.

Also postpone any object that depends on persistent multi-target state per player, persistent copy, deep runtime instance mutation, advanced central shop behavior, auction/market systems, bluff/table-position systems, or B7/B8/B9 features disguised as B6 content.

## Multi-target decision

Decision: **do not implement real multi-target execution before B6.1**.

A coherent first B6 wave can be finished without real multi-target execution by focusing on typed defenses and fixed single-target interactions. This does not close the entire B6 batch; it closes a safe V0.1-ready B6.1 slice.

Objects that remain impossible or unsafe without real multi-target execution:

- `dice.vampire`: `all_opponents` score steal.
- `item.dice.vampire`: blocked by the real behavior of `dice.vampire`.
- `module.position.lateral_pressure`: left and right opponents as two affected targets.
- `module.legendary.cursed_table`: `all_opponents` score multiplier/penalty.

These objects are not indispensable to V0.1 PC. They should be reported to B6.2 or later.

If this decision changes later, the smallest acceptable multi-target system should be:

- no long-duration target persistence;
- no replay/event-sourcing system;
- no B9 random/copy system;
- resolve targets once per effect execution;
- execute the same effect payload per resolved target;
- commit target-side deltas per affected player or die;
- write ledger entries per affected target;
- add focused tests for `all_opponents` and `target.all_dice`.

## Recommended B6.1 wave

Do not create these DataAssets in this audit pass. For the next content pass, create only the rows below, with the re-scopes documented here.

| Object | Type | Effects needed | Target rule | Negative categories | Defense path | Minimum tests | Matrix status after creation |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Bouclier / `module.defense.shield` | Module | `PreventNegativeEffect`, optional `PreventNegativeEffectBlockCount=1` | `Source` | Prevents global or authored categories | It is the defense | Blocks a negative effect; optional second negative applies if count is 1; ledger `EffectPrevented` | `validated` only after full gate |
| Coffre-Fort / `module.defense.safe` | Module | `PreventNegativeEffect` | `Source` | Prevents `GoldSteal`, `GoldLoss` | It is the defense | Gold loss/steal blocked; unrelated score/die negative still applies; ledger readable | `validated` only after full gate |
| Table Stable / `module.defense.stable_table` | Module | `PreventNegativeEffect` | `Source` | Prevents `LockModification`, `ForcedReroll` | It is the defense | Forced reroll or lock disruption blocked; no locked-die-only assumption | `validated` only after full gate |
| Protection Rare / `module.defense.rare_protection` | Module | `PreventNegativeEffect` | `Source` | Prevents all `DieDestroyOrRemove` | It is the defense | Die removal blocked for any rarity; no rare-only filtering claim | `validated` only after full gate |
| Sang-Froid / `module.competitive.composure` | Module | Re-scoped `PreventNegativeEffect` | `Source` | Prevents `ScorePenalty` and/or `ScoreSteal` | Shield/Composure blocks score harm | Score penalty/steal blocked; no 50 percent reduction text remains | `validated` only after full gate |
| Calme / `module.defense.calm` | Module | Re-scoped `PreventNegativeEffect` | `Source` | Prevents `DieValueReduction` | Shield/Calm blocks die value harm | Die value reduction blocked; no partial mitigation text remains | `validated` only after full gate |
| Audit Fiscal / `consumable.tax_audit` | Consumable | `SpendGold`, fixed amount only | Prefer `target.opponent` for explicit PC target selection, or `richest_player` only if source self-hit behavior is accepted and tested | `GoldLoss` | Safe or Shield | Target selection path works; fixed amount spends; self-target behavior impossible or explicitly tested; ledger shows applied/prevented | `validated` only after full gate |
| Taxe de Luxe / `module.competitive.luxury_tax` | Module | Separate fixed `SpendGold` on resolved player and fixed `AddGold` to source | `richest_player` for spend, `Source` for gain | `GoldLoss` on spend effect | Safe or Shield | Richest target spends fixed amount; source gains fixed amount; source-is-richest case documented and tested; ledger per effect | `validated` only after full gate |

Recommended matrix handling for the next pass:

- keep `Progress=todo` until each real DataAsset exists and passes the full validation gate;
- update `Parameters`, `ValidationNotes`, and `TechnicalRisk` for re-scoped objects before marking them validated;
- mark only the constrained B6.1 assets `validated`;
- do not mark blocked or postponed B6 rows validated because they are design-approved in abstract.

## V0.1 PC backlog

### A. Gameplay core

- Local match for 2 to 4 players.
- Player count selection.
- Round count selection.
- Start match and end match flows.
- Visible round phases: roll, lock/reroll, action, resolution, reward, ranking, shop.
- Roll, lock, reroll, action, resolution, reward, ranking, and shop all usable end to end.
- Readable score breakdown.
- Final victory by total victory points.

### B. UI PC

- Simple main menu.
- PC match creation flow.
- Minimal lobby/table setup.
- Match HUD.
- Dice UI.
- Consumable and module UI.
- Player and die target selection UI.
- Applied/blocked/invalid effect feedback.
- Ranking and reward UI.
- Shop UI: choose 1 of 3 offers.
- End-of-match screen.
- PC input polish: focus, click, hover, keyboard/mouse clarity.

Gamepad/controller navigation is explicitly outside V0.1.

### C. Debug/dev tools

- Keep the dev sandbox available.
- Keep `Scripts/Run-GambitValidation.ps1` as the local gate.
- Keep the DataAsset audit and matrix comparison.
- Add or keep smoke match checks for 2, 3, and 4 players.
- Keep logs readable enough to diagnose phase, score, shop, and target-selection failures.

### D. Content

- Finish B6.1 only.
- Do light balance after the first playable loop exists.
- Exclude B7/B8/B9 unless an explicit V0.1 feature needs a smaller, isolated subset.

### E. Non-goals for V0.1

- Online networking.
- Gamepad/controller navigation.
- Auctions.
- Advanced central shop.
- Bluff systems.
- Complex table-position mechanics.
- B9 legendary systems.
- Persistent copy/mutation systems.
- Robust random effect pools.
- Long-duration per-target multi-target state.

## Risks

- Some `CreationClass=ReadyNow` B6 rows are historically optimistic. The current audit caveats override that label.
- `richest_player` and similar automatic rules can resolve the source player. Tax effects must define and test self-hit behavior.
- The ledger is good feedback/debug state, not a general condition-history engine yet.
- Multi-target rules can be resolved, but production target-side mutation should wait for explicit per-target execution tests.
- Partial mitigation and rare-only filtering are tempting small features, but both need clear semantics and validation before content authoring.
- V0.1 scope can drift if B7/B8/B9 rows are treated as simple content rows.

## Recommended execution order

1. Land this audit and the PC-scope wording cleanup.
2. Do a B6.1 DataAsset-only pass for the eight recommended objects above.
3. In that pass, update `ObjectMatrix.csv` notes/statuses only for the assets actually created.
4. Run `Scripts/Run-GambitValidation.ps1` and keep build, audit, automation, matrix comparison, and `git diff --check` green.
5. Add or run focused B6.1 smoke tests for gold loss, score/die prevention, target selection, and defense ledger feedback.
6. Build the minimum PC playable shell: main menu, player count, round count, lobby/table setup, start match, end match.
7. Build the PC match HUD and phase screens in this order: dice roll/lock/reroll, action/consumables, target selection, resolution/score breakdown, reward/ranking, shop.
8. Add or harden smoke match validation for 2, 3, and 4 local players.
9. Do a light balance pass only after the loop is playable from menu to final victory.
10. Revisit B6.2 only if V0.1 playtests prove multi-target or history-driven interaction is needed.
