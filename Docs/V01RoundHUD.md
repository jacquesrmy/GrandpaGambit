# V0.1 PC round HUD

This pass keeps the playable PC keyboard/mouse roll, lock, and reroll slice and adds readable end-of-round feedback for resolution, rewards, ranking, and important ledger events.

## Scope

- Roll remains automatic when `UGambitRoundFlowComponent` enters `Roll`.
- During `Selection / Reroll`, each player row shows clickable dice.
- Clicking a die requests lock/unlock through `AGambitGameMode` -> `UGambitRoundFlowComponent`.
- `Reroll Unlocked` rerolls only unlocked rerollable dice for that player.
- Rerolls used, reroll limit, and remaining rerolls are shown per player.
- Invalid lock/reroll commands return typed command results from round flow and are displayed as HUD feedback.
- After resolution, each player row shows the scored combination, raw score, final score, score totals, and score debug breakdown lines already produced by scoring/effect systems.
- After reward/ranking data exists, each player row shows round gold delta, gold breakdown lines, and VP gained when available.
- After ranking exists, the HUD shows the round ranking snapshot from `AGambitGameState`.
- Each player row shows a compact important ledger summary for effect applied, effect prevented, gold changed, score modifier applied, die modified, and die removed events.
- The widget does not evaluate dice or recalculate score. It only formats `PlayerState`/`GameState` snapshots and debug/event lines.

## Ownership

- `UGambitRoundFlowComponent` owns phase validation, reroll limit validation, lock validation, reroll counters, effect hooks, and round events.
- `AGambitGameMode` exposes detailed shell command APIs while keeping the old bool wrappers for debug and input callers.
- `UGambitDiceComponent` owns runtime dice state and performs lock/reroll mutation.
- `UGambitPlayerRoundStateComponent` owns the current round score, last score breakdown, debug score/gold lines, and round event ledger.
- `AGambitGameState` owns the round ranking snapshot.
- `UGambitPCShellWidget` displays state and sends player/die command requests. It does not calculate gameplay truth.

## HUD contents

For each local player, the match HUD shows:

- player index and name;
- current phase;
- victory points, current round score, and gold;
- rerolls used / limit / remaining;
- scored combination, raw score, final score, score totals, and scoring breakdown lines after resolution;
- round gold gained/lost and reward gold breakdown lines after reward data exists;
- VP gained once ranking has assigned it;
- important ledger entries for applied/prevented effects, gold changes, score modifier applications, and die mutation/removal;
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
8. Use `Continue Phase` to preserve the existing shell progression into Action, Resolution, Reward, Ranking, Shop, and final match completion.
9. After leaving Action, confirm the Shop phase still uses the existing `Continue Phase` flow and now displays each player's resolved combination, raw/final score, scoring breakdown, gold reward details, VP gained, round ranking, and important ledger entries when present.

## Current limits

- Layout is intentionally clear and testable, not final presentation.
- No gamepad/controller navigation was added.
- No real consumable target-selection UI was added in this pass.
- No real shop purchase flow was added to the PC shell.
- No new objects or DataAssets were created.
- Resolution, reward, ranking, and ledger feedback is text-first debug/playability UI, not final presentation.
- The shell still does not pause separately on automatic Resolution/Reward/Ranking phases; their results remain visible afterward, including during Shop.
