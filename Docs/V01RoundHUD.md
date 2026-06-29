# V0.1 PC round HUD

This pass keeps the playable PC keyboard/mouse roll, lock, and reroll slice, keeps readable end-of-round feedback for resolution/rewards/ranking, and adds the first PC Action phase consumable + target-selection controls.

## Scope

- Roll remains automatic when `UGambitRoundFlowComponent` enters `Roll`.
- During `Selection / Reroll`, each player row shows clickable dice.
- Clicking a die requests lock/unlock through `AGambitGameMode` -> `UGambitRoundFlowComponent`.
- `Reroll Unlocked` rerolls only unlocked rerollable dice for that player.
- Rerolls used, reroll limit, and remaining rerolls are shown per player.
- Invalid lock/reroll commands return typed command results from round flow and are displayed as HUD feedback.
- During `Action`, each player row shows owned consumables with item label/id, stack, one-use state, and whether the item is usable in the current phase.
- Clicking `Use Slot N` calls the owning `AGambitPlayerController::RequestUseConsumable`.
- Consumables with no explicit target use the existing `GameMode` -> `RoundFlow` execution path.
- Consumables requiring a target show the pending target-selection request owned by the controller.
- Target options are displayed from the existing `FGambitTargetSelectionRequest`; the HUD does not calculate valid targets.
- PC mouse buttons allow selecting one option, confirming the selected option, or cancelling the pending request.
- A successful confirm calls the existing controller command and consumes the consumable only after round flow accepts the confirmed result.
- After resolution, each player row shows the scored combination, raw score, final score, score totals, and score debug breakdown lines already produced by scoring/effect systems.
- After reward/ranking data exists, each player row shows round gold delta, gold breakdown lines, and VP gained when available.
- After ranking exists, the HUD shows the round ranking snapshot from `AGambitGameState`.
- Each player row shows a compact important ledger summary for effect applied, effect prevented, consumable used, gold changed, score modifier applied, die modified, die removed, target selection requested/cancelled, and target selection invalid events.
- The widget does not evaluate dice or recalculate score. It only formats `PlayerState`/`GameState` snapshots and debug/event lines.

## Ownership

- `UGambitRoundFlowComponent` owns phase validation, reroll limit validation, lock validation, reroll counters, effect hooks, and round events.
- `AGambitGameMode` exposes detailed shell command APIs while keeping the old bool wrappers for debug and input callers.
- `AGambitPlayerController` owns pending target-selection state and the select/confirm/cancel commands.
- `UGambitTargetSelectionService` owns target request and option generation.
- `UGambitDiceComponent` owns runtime dice state and performs lock/reroll mutation.
- `UGambitInventoryComponent` owns consumable runtime slots and inventory instances.
- `UGambitPlayerRoundStateComponent` owns the current round score, last score breakdown, debug score/gold lines, and round event ledger.
- `AGambitGameState` owns the round ranking snapshot.
- `UGambitPCShellWidget` displays state and sends player/die/consumable/target-selection command requests. It does not calculate gameplay truth.

## HUD contents

For each local player, the match HUD shows:

- player index and name;
- current phase;
- victory points, current round score, and gold;
- rerolls used / limit / remaining;
- scored combination, raw score, final score, score totals, and scoring breakdown lines after resolution;
- round gold gained/lost and reward gold breakdown lines after reward data exists;
- VP gained once ranking has assigned it;
- important ledger entries for applied/prevented effects, consumable use, gold changes, score modifier applications, die mutation/removal, and target-selection feedback;
- owned consumables during Action, including slot number, item label/id, stack count, one-use state, and usable/unusable state;
- `Use Slot N` buttons for usable consumables during Action;
- pending target-selection summary, selected option id, invalid reason if present, and service-built target options;
- target option buttons plus `Confirm Target` and `Cancel` while a request is pending;
- one clickable button per runtime die with value and locked/open state;
- a per-player `Reroll Unlocked` button.

At the match level, the HUD also shows:

- current round ranking once `AGambitGameState` has a ranking snapshot;
- final ranking and winner when the match is complete.

The HUD also shows a top-level feedback line after lock/reroll commands. Examples:

- `Die 1 locked.`
- `Reroll used 1/2.`
- `Reroll refused: limit reached (2/2).`
- `Lock refused: current phase is Action, expected Selection / Reroll.`
- `Consumable slot 1 used.`
- `Target selection opened for slot 2.`
- `Target option 0 selected.`
- `Target confirmed.`
- `Target selection cancelled.`

## Testing from Gambit.umap

1. Open `Content/GrandpaGambit/Maps/Gambit.umap`.
2. Press Play.
3. Use `New PC Match`.
4. Choose 2, 3, or 4 local players.
5. Continue to the lobby and start the match.
6. In the `Selection / Reroll` phase:
   - click a die button to lock it;
   - click it again to unlock it;
   - click `Reroll Unlocked` and confirm locked dice keep their value;
   - confirm rerolls used and remaining update.
7. Click `Reroll Unlocked` after the limit is reached to see refusal feedback.
8. Use `Continue Phase` to enter `Action`.
9. In `Action`, inspect each player's `Consumables` section.
10. If a player owns a no-target consumable, click `Use Slot N` and confirm the item is consumed only on success and the ledger shows `ConsumableUsed`, `EffectApplied`, and any produced `GoldChanged`/die/score events.
11. If a player owns a target-rule consumable such as a `target.opponent` item, click `Use Slot N` and confirm the pending `Target Selection` section appears with service-built options.
12. Click a target option, then `Confirm Target`; verify the effect applies to the selected player and the ledger shows applied/prevented/invalid feedback when produced.
13. Repeat with a target consumable and click `Cancel`; verify the pending request clears and the consumable remains in inventory.
14. Use `Continue Phase` to preserve the existing shell progression into Resolution, Reward, Ranking, Shop, and final match completion.
15. After leaving Action, confirm the Shop phase still uses the existing `Continue Phase` flow and displays each player's resolved combination, raw/final score, scoring breakdown, gold reward details, VP gained, round ranking, and important ledger entries when present.

## Current limits

- Layout is intentionally clear and testable, not final presentation.
- No gamepad/controller navigation was added.
- Target selection is single-option confirmation over existing request options; no real multi-target selection was added.
- No real shop purchase flow was added to the PC shell.
- No new objects or DataAssets were created.
- Action, target-selection, resolution, reward, ranking, and ledger feedback is text-first debug/playability UI, not final presentation.
- The shell still does not pause separately on automatic Resolution/Reward/Ranking phases; their results remain visible afterward, including during Shop.
