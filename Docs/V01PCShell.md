# V0.1 PC playable shell

This pass adds the first minimal PC keyboard/mouse shell for a local match.

## Flow

1. Open the default map `Content/GrandpaGambit/Maps/Gambit.umap`.
2. Press Play.
3. Use the C++ PC shell overlay:
   - `New PC Match`
   - choose 2, 3, or 4 local players
   - adjust the round count
   - continue to the lobby/table setup
   - start the match
4. During the match, the shell shows the current round, phase, player VP, score, gold, dice values, rerolls used/remaining, shop offers, and minimal shop purchase feedback.
5. During `Selection / Reroll`, click dice to lock/unlock them and use `Reroll Unlocked` per player.
6. Use `Continue Phase` to mark all players ready during the current ready-gated phase:
   - Selection / Reroll
   - Action
   - Shop
7. When the configured final round ends, the shell shows the final ranking and winner.

See `Docs/V01RoundHUD.md` for the Roll / Lock / Reroll HUD contract and manual test path.

## Ownership

- `AGambitGameMode` owns shell commands and match start/reset orchestration.
- `AGambitGameState` owns visible lifecycle state, selected setup, current phase/round, and final ranking snapshots.
- `UGambitRoundFlowComponent` owns round progression, lock/reroll validation, reroll counters, and final ranking generation.
- `UGambitPCShellWidget` only displays state, sends commands, and shows command feedback.

## Launch map

`Config/DefaultEngine.ini` now points both `GameDefaultMap` and `EditorStartupMap` at `Gambit.umap`, the existing table scene. The older `L_Menu_DevMatch.umap` and `L_DevMatchSandbox.umap` remain available as debug/dev maps, but they are no longer the default PC shell entrypoint.

## Current limits

- This is not the final action/shop HUD.
- Dice lock/reroll is playable but layout is not final presentation.
- No gamepad/controller navigation was added.
- No real multi-target UI was added.
- No B6/B7/B8/B9 objects or DataAssets were added.
- Shop purchasing is now available in the PC shell as the minimal V0.1 "choose 1 offer among 3" flow; advanced shop mechanics remain outside this pass.
