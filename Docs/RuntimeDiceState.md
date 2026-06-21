# Runtime dice state

`UGambitDiceDefinition` is static authored content: faces, weights, tags, default contribution rules, lock/reroll flags, and dice effects. It must not store mutable match state.

Active dice use `FGambitDieRuntimeState` through `UGambitDiceComponent`. The runtime state owns the match/round instance id, hand index, original definition from the loadout, current effective definition, current value, lock state, temporary transform flags, temporary/removal flags, contribution overrides, runtime tags, and lightweight source item/effect trace data.

Round reset rebuilds the active dice runtime array from the player loadout. This clears temporary transforms, temporary dice, runtime tags/effect traces, locks, round-local value changes, and removed-round markers. Stable instance ids are preserved by hand index where possible so reroll/event tracking can stay readable across round rebuilds.

Current fallback transforms without a `TransformDiceDefinition` preserve the requested current value and tag the runtime die with the requested dice type, but they do not generate a real temporary face table from min/max. Rerolls still use the fallback D6 path until custom per-instance faces are implemented.

Persistent mutations, generated per-instance face tables, and opponent copy effects remain out of scope for now.
