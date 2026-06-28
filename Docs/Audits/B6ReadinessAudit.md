# B6 Readiness Audit

Date: 2026-06-27

Scope: audit only. No gameplay code, DataAssets, ObjectMatrix rows, or validation scripts were changed in this pass.

## Global Status

**Ready with caveats** for a first controlled wave of real B6 objects.

The project is ready to author B6 content that stays inside the existing effect resolver, typed negative defenses, target rules, ledger feedback, configurable scoring, runtime dice state, runtime inventory instances, shop/shared-pool rules, and DataAsset validation.

It is not ready to create the whole B6 batch blindly. Objects that depend on true multi-target mutation, persistent per-player target state, final PC keyboard/mouse UI and focus/click/hover polish, previous-round/win-streak snapshots, random removal, partial mitigation, advanced cooldowns, or object mutation should remain out of scope.

## Sources Reviewed

- `Docs/ObjectCreation/README.md`
- `Docs/ObjectCreation/CreationBatches.md`
- `Docs/ObjectCreation/EffectsCoverage.md`
- `Docs/ObjectCreation/ObjectMatrix.csv`
- `Docs/TargetSelectionFlow.md`
- `Docs/RoundGameplayEvents.md`
- `Docs/Scoring.md`
- `Docs/RuntimeDiceState.md`
- `Docs/InventoryRuntimeInstances.md`
- `Docs/Audits/DataAssetsAudit.md`
- `Saved/Automation/GambitDataAssetsAudit.json`
- `Saved/Automation/GambitDataAssetsAudit.csv`

## Validation Run

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
| `git diff --check` | OK |

Run logs:

```text
Saved/Automation/GambitValidation/20260627-205937/
```

Current audit output files:

```text
Saved/Automation/GambitDataAssetsAudit.json
Saved/Automation/GambitDataAssetsAudit.csv
```

Audit comparison summary:

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

The 139 planned missing rows are backlog/planning entries, including B6 and later objects. They are informational, not validation failures.

## Readiness By System

| Area | Readiness | Notes |
| --- | --- | --- |
| Negative effects and defenses | Ready with caveats | `bNegativeEffect`, `NegativeEffectCategories`, typed `PreventedNegativeEffectCategories`, and simple `PreventNegativeEffectBlockCount` exist. Validation warns on unsafe category authoring. Use full prevention by category. Do not author partial reduction, rare-only defense, or advanced cooldown behavior yet. |
| Player and die targeting | Ready with caveats | Target rules cover explicit opponent, selected die, source/target die rules, leader/richest/poorest/lowest score, order-based opponents, random opponent, and all-opponents metadata. Consumable target selection has a debug/dev flow. Caveat: runtime mutation paths currently resolve a target result but generally apply to one resolved target, so do not rely on `all_opponents` for real score/gold/die mutations until a dedicated multi-target execution test exists. |
| Feedback and ledger | Ready | Round events cover applied/prevented effects, consumable use, gold changes, score modifiers, die mutation/removal, rerolls, scoring, and target-selection feedback. Caveat: ledger is round-local feedback/debug state, not replay, telemetry, or persistent condition storage. |
| Scoring | Ready | Base combination scores are configurable, score modifier math is centralized, and automation covers fallback/default scores and modifier merge. Avoid objects that need circular "rank before score" behavior, previous-round winner state, or unexplained score floors. |
| Shop and inventory | Ready with caveats | Shop pricing, cashback, free offers, shared-pool reservation, purchase flow, and runtime inventory item instances are covered. Avoid advanced charges/cooldowns, persistent item mutation, persistent copies, and random module/die removal unless a specific supported path is tested. |
| Dice runtime | Ready with caveats | Runtime dice state, effective definitions, temporary face overrides, contribution flags, removal marks, and round reset are documented and tested. Avoid persistent face mutation, opponent copy dice, and generated random per-instance face tables beyond current min/max transforms. |
| Tests | Ready | The full local gate passes and contains focused coverage for target rules, target selection, negative protection, ledger, scoring, dice runtime, inventory instances, shop/shared pool, and prior B3/B4/B5 smoke paths. |

## Systems Ready For B6 Content

- DataAsset audit and matrix reconciliation can detect invalid assets, unknown target rules, category mistakes, missing outputs, type mismatches, status mismatches, duplicate IDs, and real production assets absent from the matrix.
- Typed negative defense authoring is viable for category-based full blocks.
- Single-target offensive effects are viable when they use supported effect types, explicit target semantics, and clear ledger feedback.
- Consumables that require a player or die target can use the target selection flow, with debug UI/state support.
- Score, gold, dice runtime, shop, shared-pool, and inventory effects are sufficiently separated for DataAsset authoring within existing effect types.

## Caveats

- B6 is not one homogeneous "ready" batch. The matrix still contains several B6 rows whose design asks for systems that are not ready.
- `CreationClass=ReadyNow` in `ObjectMatrix.csv` must be interpreted with the current caveats. Some rows have historical risk notes that are partly superseded, but other risks remain real.
- Multi-target rules are recognized and resolvable, but production mutations should stay single-target until the resolver has explicit multi-target execution coverage.
- Richest/leader target rules can resolve the source player. Any tax/sabotage object must define whether self-hit is allowed.
- The ledger records what happened; it does not yet provide a general condition system like "if you were stolen from last round, gain X".
- Final PC UX for target selection, focus, click/hover states, and keyboard/mouse interaction is still not production-ready. Gamepad/controller navigation is future work outside V0.1 scope.

## B6 Objects Authorized Now

Author only the constrained versions below:

- `module.defense.shield`: category or global `PreventNegativeEffect`; simple full block. If using a one-block limit, verify `PreventNegativeEffectBlockCount` in a focused smoke test.
- `module.defense.safe`: full prevention for `GoldSteal` and/or `GoldLoss`.
- `module.defense.stable_table`: full prevention for lock/reroll disruption categories such as `LockModification` and `ForcedReroll`, not per-die locked-only logic.
- `module.defense.rare_protection`: only if re-scoped to full `DieDestroyOrRemove` protection. Do not ship rare-only filtering yet.
- `consumable.tax_audit`: fixed-amount `SpendGold` against one resolved player. Do not use percent-of-current-gold. Define and test self-target behavior if using `richest_player`.
- `module.competitive.luxury_tax`: fixed `SpendGold`/`AddGold` with one resolved player. Do not use `all_opponents` for production behavior yet. Define and test self-target behavior.
- `module.competitive.composure` and `module.defense.calm`: only if re-scoped to full category prevention. Do not author "reduce negative effect by 50%" semantics yet.

## B6 Objects To Avoid For Now

- `dice.vampire`, `module.competitive.point_thief`, `module.position.lateral_pressure`, and `module.legendary.cursed_table` when they require true `all_opponents` score steal or multi-target mutation.
- `item.dice.vampire` until `dice.vampire` has a supported single-target or definition-only implementation.
- `module.competitive.light_saboteur` as a passive module if it depends on final opponent-die selection UI. A consumable version with target selection would be safer than a passive module.
- `module.rarity.monopoly`, `module.shared.disputed_collection`, and `module.competitive.jealous_copycat` because they need opponent inventory or score-breakdown comparison/copy semantics.
- `module.competitive.rivalry` because it needs persistent rival selection.
- `module.mult.medium_bet`, `module.competitive.vengeance`, `module.competitive.table_king`, `module.competitive.first_blood`, `module.competitive.last_word`, `module.competitive.false_weak`, and `module.legendary.final_boss` when they rely on delayed next-round state, previous-rank/win-streak snapshots, turn order, mid-round ranking, or rank-before-score semantics.
- `consumable.risky_contract` and `module.risk.cursed_pact` when they require random module/die removal. Current sell/remove paths are specific, not a production random-removal flow.
- `module.risk.roulette` because random effect choice is not a robust DataAsset-driven effect pool yet.
- `module.defense.anti_theft` and `module.competitive.insurance` because the ledger records steal/gold events, but there is not yet a general authored condition that pays out from "was stolen from" and amount history.
- Any B6 object needing partial mitigation, score floors, rare-only defense filtering, persistent per-player multi-target effects, cooldowns, or item-instance mutation.

## Tests And Validations To Rerun Per Content Wave

Always run the full gate:

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Run-GambitValidation.ps1
```

For every B6 wave, confirm:

- Build stays OK.
- `Saved/Automation/GambitDataAssetsAudit.json` and `.csv` are regenerated and non-empty.
- DataAsset audit reports 0 warnings and 0 errors.
- Matrix comparison reports 0 actual production assets missing from matrix, 0 type mismatches, 0 status mismatches, and 0 duplicate actual IDs.
- Automation report has 0 warnings, 0 failed, 0 notRun, and 0 inProcess.
- `git diff --check` stays OK.

Key automation coverage to watch:

- `GrandpaGambit.Effects.NegativeProtection.Categories`
- `GrandpaGambit.Effects.NegativeProtection.Validation`
- `GrandpaGambit.Effects.TargetRules.Recognition`
- `GrandpaGambit.Effects.TargetResolver.ResolveTargets`
- `GrandpaGambit.Effects.TargetResolver.V2Rules`
- `GrandpaGambit.Effects.TargetResolver.NonDiceExecution`
- `GrandpaGambit.Items.TargetRuleValidation`
- `GrandpaGambit.TargetSelection.OpponentOptions`
- `GrandpaGambit.TargetSelection.DieOptionsAndCancel`
- `GrandpaGambit.TargetSelection.AppliesAndReportsFeedback`
- `GrandpaGambit.RoundEvents.Ledger`
- `GrandpaGambit.Scoring.DefaultCombinationScores`
- `GrandpaGambit.Scoring.CombinationScoreFallback`
- `GrandpaGambit.Scoring.ScoreModifierMath`
- `GrandpaGambit.Shop.*`
- `GrandpaGambit.Inventory.RuntimeInstances`
- `GrandpaGambit.Dice.RuntimeStateLifecycle`
- `GrandpaGambit.Dice.RuntimeFaceOverride`
- B3/B4/B5 smoke tests that cover consumables, economy, reroll, shared pool, and rare dice behavior.

For a wave containing offensive B6 content, add or manually run a 4-player local smoke check before marking objects `validated`: attack applies to the intended target, defense blocks the expected category, ledger feedback is readable, and the round still completes through shop/ranking.

## Remaining Debt Not Treated Here

- Final PC UI for target selection, including focus, click/hover, and keyboard/mouse polish. Gamepad/controller navigation is future work outside V0.1 scope.
- Persistent per-player multi-target effects.
- Advanced charges/cooldowns beyond simple negative-effect block counts.
- Previous-round/winner/streak snapshots for comeback or vengeance designs.
- General event-history conditions powered by the ledger.
- Random effect pools and random inventory/die removal flows.

## Decision

Proceed with a narrow first B6 content wave focused on full typed defenses and simple single-target fixed-value interactions. Do not create broad multi-target sabotage, persistent revenge, partial mitigation, random removal, or advanced stateful objects in this pass.
