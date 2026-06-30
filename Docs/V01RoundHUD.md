# V0.1 PC round HUD

This pass keeps the playable PC keyboard/mouse roll, lock, and reroll slice, keeps readable end-of-round feedback for resolution/rewards/ranking, keeps the first PC Action phase consumable + target-selection controls, and adds the minimal PC Shop purchase flow.

## Status

This HUD is the text-first surface inside `UGambitPCShellWidget`. It exists to
keep V0.1 playable on PC keyboard/mouse and is not the final match HUD.

Future final UI work should replace this presentation and consume the stable
production UI contract snapshots/commands where applicable. Do not add gameplay
truth, final layout policy, animation policy, focus policy, or gamepad/navigation
policy to this shell HUD.

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
- After resolution, each player row shows the scored combination, raw score, final score, score totals, and score breakdown lines already produced by scoring/effect systems.
- After reward/ranking data exists, each player row shows round gold delta, production gold breakdown lines, and VP gained when available.
- After ranking exists, the HUD shows the round ranking snapshot from `AGambitGameState`.
- During `Shop`, each player row shows that player's generated offers.
- Shop offers show display name, stable item id, item type, rarity, gold cost, shared-pool state when relevant, and whether the offer is currently buyable.
- Clicking `Buy Offer N` calls `AGambitGameMode::RequestPurchaseOfferDetailed`, which delegates to `UGambitRoundFlowComponent`.
- Purchase validation remains gameplay-owned and refuses out-of-phase purchases, invalid offers, insufficient gold, shared-pool unavailability, full slots, effect-blocked purchases, and a second purchase in the same shop.
- A successful purchase spends gold, commits shared-pool stock when needed, grants the item through `UGambitInventoryComponent`, records the runtime inventory instance, and updates shop/gold debug feedback.
- During `Shop`, each player row also shows current gold and an inventory summary so gold and inventory changes are visible immediately after purchase.
- Each player row shows a compact important ledger summary for effect applied, effect prevented, consumable used, gold changed, score modifier applied, die modified, die removed, target selection requested/cancelled, and target selection invalid events.
- The widget does not evaluate dice or recalculate score. It only formats `PlayerState`/`GameState` snapshots and debug/event lines.

## Ownership

- `UGambitRoundFlowComponent` owns phase validation, reroll limit validation, lock validation, reroll counters, effect hooks, and round events.
- `UGambitRoundFlowComponent` owns shop purchase phase validation, one-purchase-per-shop V0.1 validation, purchase effect hooks, and shop offer snapshots for the HUD.
- `AGambitGameMode` exposes detailed shell command APIs while keeping the old bool wrappers for debug and input callers.
- `AGambitPlayerController` owns pending target-selection state and the select/confirm/cancel commands.
- `UGambitTargetSelectionService` owns target request and option generation.
- `UGambitDiceComponent` owns runtime dice state and performs lock/reroll mutation.
- `UGambitInventoryComponent` owns consumable runtime slots and inventory instances.
- `UGambitShopComponent` owns offer storage, purchase preview validation, price resolution, gold spend, shared-pool commit, and inventory grant mechanics.
- `UGambitPlayerRoundStateComponent` owns the current round score, last score breakdown, score/gold breakdown lines, and round event ledger.
- `AGambitGameState` owns the round ranking snapshot.
- `UGambitPCShellWidget` displays state and sends player/die/consumable/target-selection/shop command requests. It does not calculate gameplay truth.

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
- shop offers during Shop, including offer id, item label/id, type, rarity, cost, shared-pool state, and buyable/unavailable reason;
- `Buy Offer N` buttons for buyable offers during Shop;
- current shop purchase count, current gold, and inventory summary during Shop;
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
- `Purchased Shop Coffee for 5 gold. Gold 20 -> 15.`
- `Purchase refused: Not enough gold: Gold=0 Price=50 MinGold=0.`
- `Purchase refused: this player already bought an offer this shop.`

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
14. Use `Continue Phase` to progress into Resolution, Reward, Ranking, and Shop.
15. In `Shop`, inspect each player's offer section and verify every offer shows label/id/type/rarity/cost and a buyable or unavailable state.
16. Click one buyable `Buy Offer N` button for a player.
17. Confirm the feedback line reports the purchase, current gold decreases, the inventory summary updates, and the bought item has a runtime inventory instance through the existing inventory system.
18. Try to buy a second offer for the same player and confirm the HUD refuses it as an already-completed shop purchase.
19. If an offer is unaffordable or shared-pool unavailable, confirm the button is disabled or the command feedback reports the gameplay-owned refusal reason.
20. Use `Continue Phase` in Shop and confirm the flow reaches the next round for 2, 3, and 4 player matches, or final match completion when the configured final round ends.

## Current limits

- Layout is intentionally clear and testable, not final presentation.
- No gamepad/controller navigation was added.
- Target selection is single-option confirmation over existing request options; no real multi-target selection was added.
- Shop is the minimal V0.1 one-purchase flow only: no reroll shop, central market, auctions, reservation UI, non-gold costs, or advanced sell/replace flow.
- No new objects or DataAssets were created.
- Action, target-selection, resolution, reward, ranking, shop, and ledger feedback is text-first playability UI, not final presentation.
- The shell still does not pause separately on automatic Resolution/Reward/Ranking phases; their results remain visible afterward, including during Shop.
