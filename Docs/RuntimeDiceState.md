# Runtime dice state

`UGambitDiceDefinition` is static authored content: faces, weights, tags, default contribution rules, lock/reroll flags, and dice effects. It must not store mutable match state.

Active dice use `FGambitDieRuntimeState` through `UGambitDiceComponent`. The runtime state owns the match/round instance id, hand index, original definition from the loadout, current effective definition, optional temporary runtime faces, current value, lock state, temporary transform flags, temporary/removal flags, contribution overrides, runtime tags, and lightweight source item/effect trace data.

Round reset rebuilds the active dice runtime array from the player loadout. This clears temporary transforms, temporary dice, runtime tags/effect traces, locks, round-local value changes, and removed-round markers. Stable instance ids are preserved by hand index where possible so reroll/event tracking can stay readable across round rebuilds.

`TransformDiceDefinition` is the authored-content path. When an effect supplies it, the die's effective definition changes for the current round, authored faces and weights come from that definition, and the runtime face override stays disabled.

When a temporary transform has no `TransformDiceDefinition`, the component builds a per-instance runtime face table from the effect min/max range (`min..max`). `bHasRuntimeFaceOverride` marks that the override is active, and `RuntimeFaces` stores the rollable values. Future rolls and rerolls use those faces before checking the effective `DiceDefinition`; if no runtime override exists, standard dice still roll through their effective definition, then the existing D6 fallback.

Runtime faces are round-scoped only. Round reset rebuilds dice from the loadout and clears `RuntimeFaces`, `bHasRuntimeFaceOverride`, runtime source ids, applied effect traces, and `bTemporarilyTransformed`.

Persistent mutations, opponent copy effects, persistent multi-target runtime context, and generated random per-instance face tables beyond simple min/max transforms remain out of scope for now.
