# V0.1 PC playable shell

This pass adds the first minimal PC keyboard/mouse shell for a local match.

## Flow

1. Open the default map `Content/GrandpaGambit/Maps/L_Menu_DevMatch.umap`.
2. Press Play.
3. Use the C++ PC shell overlay:
   - `New PC Match`
   - choose 2, 3, or 4 local players
   - adjust the round count
   - continue to the lobby/table setup
   - start the match
4. During the match, the shell shows the current round, phase, player VP, score, gold, dice values, and shop offers.
5. Use `Continue Phase` to mark all players ready during the current ready-gated phase:
   - Selection / Reroll
   - Action
   - Shop
6. When the configured final round ends, the shell shows the final ranking and winner.

## Ownership

- `AGambitGameMode` owns shell commands and match start/reset orchestration.
- `AGambitGameState` owns visible lifecycle state, selected setup, current phase/round, and final ranking snapshots.
- `UGambitRoundFlowComponent` owns round progression and final ranking generation.
- `UGambitPCShellWidget` only displays state and sends commands.

## Current limits

- This is not the final dice/action/shop HUD.
- No gamepad/controller navigation was added.
- No real multi-target UI was added.
- No B6/B7/B8/B9 objects or DataAssets were added.
- Shop purchasing is still available through existing dev/debug paths; the shell only displays offers and skips shop via `Continue Phase`.
