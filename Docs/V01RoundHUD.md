# V0.1 PC round HUD

This pass adds the first playable PC keyboard/mouse HUD slice for roll, lock, and reroll.

## Scope

- Roll remains automatic when `UGambitRoundFlowComponent` enters `Roll`.
- During `Selection / Reroll`, each player row shows clickable dice.
- Clicking a die requests lock/unlock through `AGambitGameMode` -> `UGambitRoundFlowComponent`.
- `Reroll Unlocked` rerolls only unlocked rerollable dice for that player.
- Rerolls used, reroll limit, and remaining rerolls are shown per player.
- Invalid lock/reroll commands return typed command results from round flow and are displayed as HUD feedback.

## Ownership

- `UGambitRoundFlowComponent` owns phase validation, reroll limit validation, lock validation, reroll counters, effect hooks, and round events.
- `AGambitGameMode` exposes detailed shell command APIs while keeping the old bool wrappers for debug and input callers.
- `UGambitDiceComponent` owns runtime dice state and performs lock/reroll mutation.
- `UGambitPCShellWidget` displays state and sends player/die command requests. It does not calculate gameplay truth.

## HUD contents

For each local player, the match HUD shows:

- player index and name;
- current phase;
- victory points, current round score, and gold;
- rerolls used / limit / remaining;
- one clickable button per runtime die with value and locked/open state;
- a per-player `Reroll Unlocked` button.

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

## Current limits

- Layout is intentionally clear and testable, not final presentation.
- No gamepad/controller navigation was added.
- No real consumable target-selection UI was added in this pass.
- No real shop purchase flow was added to the PC shell.
- No new objects or DataAssets were created.
