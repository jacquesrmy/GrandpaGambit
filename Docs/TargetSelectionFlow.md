# Target Selection Flow

This is the first dev-facing foundation for explicit player/die target selection.

## Runtime flow

1. A local player requests a consumable through `AGambitPlayerController::RequestUseConsumable`.
2. `AGambitGameMode` forwards target-selection request building to `UGambitRoundFlowComponent`.
3. `UGambitRoundFlowComponent` uses `UGambitTargetSelectionService` to inspect the consumable's `ConsumableUse` effects and existing target rules.
4. If no explicit target is required, the old consumable flow runs unchanged.
5. If a target is required, the controller stores a pending `FGambitTargetSelectionRequest`.
6. UI selects an `FGambitTargetSelectionOption`, then calls confirm or cancel.
7. Confirm builds `FGambitTargetSelectionResult` and calls the existing round flow consumable execution with the selected player/die.
8. The consumable is consumed only after confirmation succeeds.

## State ownership

Pending selection state lives on `AGambitPlayerController`:

- `HasPendingTargetSelection`
- `GetPendingTargetSelectionRequest`
- `GetPendingTargetSelectionSelectedOptionId`
- `RequestSelectTargetSelectionOption`
- `RequestConfirmTargetSelection`
- `RequestCancelTargetSelection`

The dev sandbox snapshot mirrors this state per player slot so Blueprint debug UI can display it.

## UI contract

UI may:

- display the pending request debug text;
- display valid options returned by the service;
- highlight the selected option id;
- call select, confirm, and cancel commands;
- display round/debug events after effects apply, fail, or get prevented.

UI must not:

- calculate valid targets;
- inspect DataAssets to infer targeting rules;
- consume items directly;
- mutate dice, gold, score, shared pool, or defenses directly.

## Feedback

Target selection writes debug and round events for:

- selection requested;
- selection cancelled;
- invalid/no-option selection.

Effect application feedback remains in the existing resolver/ledger path:

- `EffectApplied`;
- `EffectPrevented`;
- `ConsumableUsed`;
- `GoldChanged`;
- `ScoreModifierApplied`;
- `DieModified`;
- `DieDestroyedOrRemoved`.

## Current limits

- This is a debug/dev UI foundation, not final presentation.
- No final animations, PC focus management, click/hover polish, or keyboard/mouse presentation model is included. Gamepad/controller navigation is future work outside V0.1 scope.
- Multi-target persistent per-player selection is not implemented.
- Automatic target rules can expose preview dice, but confirmation still passes one selected player/die context into the existing resolver.
