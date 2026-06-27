# Inventory Runtime Instances

Grandpa Gambit separates static item definitions from player-owned runtime inventory instances.

## Static Definitions

Static definitions are authoring data. They live in DataAssets and remain immutable during play:

- `UGambitItemDefinition` and subclasses define modules, consumables, and dice shop items.
- `UGambitDiceDefinition` defines dice behavior and faces.
- Definitions own display data, rarity, cost, tags, effect definitions, and balancing values.

Gameplay should not write mutable per-player state into these assets.

## Runtime Instances

`FGambitInventoryItemInstance` is the lightweight per-player owned item record stored by `UGambitInventoryComponent`.

Each instance has:

- a stable `InstanceId` for the owning player's inventory;
- an optional `ItemDefinition`;
- an optional `DiceDefinition` for owned dice and dice items;
- an `ItemType`;
- a `SourceStableId` for the static source that created the instance;
- optional purchase/effect source ids;
- simple slot state such as stack count, equipped, active, and runtime tags.

The historical APIs still return definitions for existing gameplay paths. The runtime instance registry is kept in sync so future systems can address a specific owned item without mutating DataAssets or guessing by definition pointer.

## Current Scope

This pass does not implement advanced charges, cooldowns, persistent copies, item mutation, or persistent per-player multi-target effects. Those systems should build on `FGambitInventoryItemInstance` later instead of adding mutable state to item definitions.
