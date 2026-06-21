# Round gameplay events

Grandpa Gambit keeps a lightweight per-player round event ledger in `UGambitPlayerRoundStateComponent`.

## What it records

The ledger stores structured, serializable `FGambitRoundGameplayEvent` entries for selected high-value gameplay actions:

- effect applied
- effect prevented by negative-effect protection
- consumable used through the effect resolver
- score modifier applied
- gold changed by an effect
- die modified, rerolled by an effect, or removed/marked for removal
- reroll used by the round flow
- round score finalized

Each entry carries the event type, outcome, round number, phase, source/target player ids when available, source item id, effect id/type id, target rule id, negative category ids when applicable, numeric delta, optional die ids/indexes, and a short reason.

## What it does not do

This is not a replay system, telemetry pipeline, analytics sink, or authoritative event-sourcing model. It does not store enough state to reconstruct a full match, and it should not replace direct gameplay state owned by player components, game state, inventory, dice, economy, shop, or scoring systems.

The ledger is intended for future conditional effects, defensive checks, and round-local debug audits. Query helpers stay intentionally small: get all events, filter by type, source item, effect id, or target player, and count/has checks by type.

## Known debt kept out of this pass

Real multi-target persistent effects tracked per affected player are still not implemented here. This ledger can record that a multi-target effect happened, but it does not solve persistent per-target ownership or lifecycle for those effects.
