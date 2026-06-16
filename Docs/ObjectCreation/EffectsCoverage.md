# Effects Coverage

Cette page relie la liste d'objets aux capacités C++ déjà présentes dans le projet. Elle sert à éviter de créer des DataAssets qui semblent configurables mais qui ne peuvent pas encore s'exécuter correctement.

## Déjà couvert par les enums existants

Hooks utiles déjà disponibles :

- Round : `RoundStart`, `PreRoll`, `PostRoll`, `PreReroll`, `PostReroll`, `RoundEnd`
- Score : `PreCombinationEvaluation`, `PostCombinationEvaluation`, `PreScoreCalculation`, `ScoreModifier`, `PostScoreCalculation`
- Économie/ranking : `Reward`, `Ranking`
- Boutique : `PreShopGenerate`, `PostShopGenerate`, `PrePriceResolve`, `PostPriceResolve`, `PrePurchase`, `PostPurchase`, `ShopSkipped`
- Consommables : `ConsumableUse`

## Effect execution ownership

- `UGambitRoundFlowComponent` owns phase timing and decides when a hook should run.
- `UGambitRoundEffectPipeline` builds round effect contexts, runs active module/dice/item effects, commits debug output and temporary deltas.
- `UGambitEffectResolver` remains the atomic effect resolver; it applies one item/dice effect payload to the provided context.
- Inside the resolver, private domain helpers apply score, dice, economy, shop, shared pool, defense, and utility effects. The public resolver entry points and target-rule resolution remain the integration boundary.

Effect types directement utiles :

- Score : `ScoreModifier`, `AddScoreFlat`, `MultiplyScore`, `AddTemporaryScoreModifier`
- Gold : `AddGold`, `SpendGold`, `StealGold`, `AddRecurringGoldIncome`
- Dés runtime : `ModifyDieValue`, `SetDieValue`, `LockDie`, `TransformDiceForRound`, `AddTemporaryDie`, `SetDieComboContributionCount`, `SetDieCountsForScoreSum`, `SetDieCountsForCombinations`, `SetDieCanBeRerolled`, `SetDieCanBeLocked`, `MarkDieDestroyedAfterRound`
- Reroll : `AddReroll`, `ModifyRerollLimit`
- Défense : `PreventNegativeEffect`
- Shop : `ModifyShopOfferCount`, `AddShopDiscountPercent`, `AddShopSurchargePercent`, `AddShopFlatPriceDelta`, `MakeShopOfferFree`, `AddShopCashbackPercent`, `AddPurchaseGoldDelta`, `BlockShopPurchase`
- Économie settings : `ModifyDebtLimit`, `ModifyMaxGold`, `ModifyInterestInterval`, `ModifyMaxInterest`, `ModifyInterestBonus`
- Shared pool : `AddSharedPoolStock`, `SetSharedPoolPurchaseLimit`, `SetSharedPoolItemUnavailable`
- Copie simple : `CopyLastTriggeredEffect`

## Negative effect categories and defenses

`bNegativeEffect` remains the flag that marks an effect as harmful. New assets should also set `NegativeEffectCategories` so defenses can filter what they block.

Available categories:

- `Generic`
- `GoldSteal`
- `GoldLoss`
- `ScorePenalty`
- `ScoreSteal`
- `DieValueReduction`
- `DieDestroyOrRemove`
- `ForcedReroll`
- `LockModification`
- `ShopBlock`
- `SharedPoolSabotage`

Configuration rules:

- For a negative effect, fill `NegativeEffectCategories` with one or more concrete categories.
- If `bNegativeEffect` is true and `NegativeEffectCategories` is empty, the resolver falls back to `Generic`; validation emits a warning.
- For a defense, use `EffectType = PreventNegativeEffect` and fill `PreventedNegativeEffectCategories` with the categories it can block.
- A `PreventNegativeEffect` defense with an empty `PreventedNegativeEffectCategories` list is intentionally global and keeps legacy behavior.
- `PreventNegativeEffectBlockCount = 0` means unlimited blocks within the current effect context; values above 0 consume one charge per blocked effect.
- Do not put `None` inside either category array. Leave the array empty for fallback/global behavior.

## Target rules available

`TargetRuleId` remains a stable `FName` field for existing DataAssets. Empty `TargetRuleId` keeps legacy source/target routing from `Target`.

Player rules:

- `target.opponent`: explicit target player, must be different from the source.
- `all_opponents`: every match player except the source. Multi-target.
- `leading_player`: highest victory points, then highest current round score, then stable match order.
- `richest_player`: highest current gold, ties use stable match order.
- `poorest_player`: lowest current gold, ties use stable match order.
- `lowest_score_player`: lowest current round score, ties use stable match order.
- `left_opponent`: previous player in stable match order, excluding self.
- `right_opponent`: next player in stable match order, excluding self.
- `opposite_player`: player at index +2 in exactly four-player stable match order.
- `random_opponent`: random opponent from stable match order using the effect context random stream.

Die rules:

- `selected_die`: selected die on the requested source/target side.
- `source.selected_die`
- `target.selected_die`
- `first_rerolled_die`
- `first_rerolled_die_this_round`
- `source.random_die`
- `source.best_die`
- `source.lowest_die`
- `source.all_dice`: all source dice. Multi-target at dice-index level.
- `target.random_die`
- `target.best_die`
- `target.lowest_die`
- `target.all_dice`: all target dice. Multi-target at dice-index level.

Known limits:

- Table position currently uses the stable `AGambitGameState::PlayerArray` order, copied into the effect context by round flow. This is a gameplay-order fallback until explicit table seats exist.
- `opposite_player` is valid only with exactly four players.
- `best_die` and `lowest_die` use `EffectiveValue`; ties use the lower hand index.
- Multi-player rules are resolved by `GambitEffectTargetResolver`. Effect types that persist target-side temporary context should be rechecked before using them as true multi-player effects.

Examples:

- Offensive consumable: `EffectType = StealGold`, `Target = Target`, `TargetRuleId = target.opponent`, `NegativeEffectCategories = GoldSteal`.
- Multi-opponent pressure: `EffectType = SpendGold`, `Target = Target`, `TargetRuleId = all_opponents`.
- Defense: `EffectType = PreventNegativeEffect`, `Target = Source`, empty `TargetRuleId`; fill `PreventedNegativeEffectCategories` for typed defense or leave it empty for legacy global defense.
- Die manipulation: `EffectType = ModifyDieValue`, `TargetRuleId = source.best_die` or `target.lowest_die`.

Ajouts B2 finalises :

- Score : `MultiplyDiceContribution` ajoute un bonus de contribution par de dans le breakdown, sans multiplier le score final.
- Conditions : `AllDiceUsedBySelectedCombination`, `DiceValuesPalindrome`, compteurs d'inventaire par type/duplicat/rarete et set de raretes.
- Scalaires : `AmountPerDistinctOwnedDiceType`, `AmountPerMostRepeatedOwnedDiceType`, `AmountPerOwnedDiceRarityCount`, `AmountPerOwnedItemRarityCount`, `AmountPerOwnedModuleRarityCount`.
- Multipliers : `MultiplierDeltaPerMatchingDie`, `MultiplierPerOwnedDiceRarityCount`, `MultiplierDeltaPerOwnedDiceRarityCount`.

Conditions déjà utiles :

- Valeurs de dés : `DieValueEquals`, `DieValueGreater`, `DieValueLower`, `HasAtLeastMatchingDice`
- Combos : `HasCombinationType`
- Économie/ranking/reroll : `GoldThreshold`, `RankCondition`, `RerollsUsedCondition`
- Tags/rarete : `ItemRarity`, `ItemTag`, `ShopItemRarity`, `ShopItemTag`
- Shop : `ShopPrice`, `ShopPurchaseCount`, `ShopOfferUsesSharedPool`
- Aléatoire : `ChancePercentage`

## À ajouter pour couvrir beaucoup d'objets B2-B5

Ces ajouts sont petits et très rentables :

- Conditions de parité : count even, count odd, all even, all odd.
- Conditions all-dice : all values equal X, all values in set, all values within range.
- Conditions de moyenne : die below/above average, average threshold.
- Conditions de score brut : raw score threshold before modifiers.
- Compteurs dynamiques restants : per threshold crossed et compteurs hors inventaire B2.
- Reroll delta tracking : value increased, value decreased, no value changed, first/last rerolled die.
- Target rules de base : covered by `selected_die`, `source.*` die rules and `target.*` die rules; remaining work is explicit UI for player-selected opponent dice.
- Breakdown combo enrichi restant : les indexes et le flag all-dice-used existent; il reste surtout le feedback UI.

## À ajouter pour les objets B6

Ces ajouts concernent surtout l'interaction locale :

- Target rules adverses : selected opponent die only remains; player order rules now exist as `TargetRuleId` values.
- Typage des effets négatifs : steal gold, steal score, reduce die, destroy die, block shop, force reroll.
- Event history : steal event, gold gained event, consumable used event, effect triggered event.
- Previous round snapshots : previous rank, previous winner, win streak, previous score.

## À ajouter pour les objets B7-B9

Ces systèmes sont de vrais chantiers, pas de simples DataAssets :

- Timer par joueur et hooks de temps restant.
- Placement des dés autour de la table : ordre, zones, adjacence, géométrie, visibilité.
- UI de choix : choisir face, choisir rival, choisir pair/impair, vendre un dé, cibler un dé adverse.
- Shop avancé : boutique centrale, enchères, réservation, offres générées dynamiquement, coûts non-gold.
- Moteur d'état persistant par instance de dé : croissance de faces, multiplicateur cumulatif, historique par die instance.
- Pipeline de multiplicateurs avec caps, annulation, modification du prochain multiplicateur et explication dans le breakdown.
- Moteur d'effets aléatoires/copiés avec garde anti-boucle.

## TargetRule authoring

Use only IDs returned by `UGambitItemEffectDefinition::GetTargetRuleIdOptions()`. Validation reports unknown IDs as errors and reports conservative warnings for obvious mismatches, such as a die rule on an effect type that does not directly mutate dice.


## EffectTypeId recommandé pour custom effects

Quand l'enum `EGambitItemEffectType` ne suffit pas, garder le `EffectType` proche de l'existant et utiliser `EffectTypeId` pour préciser :

- `effect_type.dynamic_score_from_gold`
- `effect_type.per_die_multiplier`
- `effect_type.parity_count`
- `effect_type.average_condition`
- `effect_type.reroll_delta`
- `effect_type.table_position`
- `effect_type.timer_based_score`
- `effect_type.copy_opponent_asset`
- `effect_type.persistent_die_mutation`
- `effect_type.random_effect_pool`
- `effect_type.auction_flow`

## Validation minimale par asset

Avant de cocher `validated` :

- L'asset passe `IsDataValid`.
- Les IDs ne sont pas vides et restent uniques.
- Les effets adverses ont un `TargetRuleId`.
- Les shared pool items ont `SharedPoolMaxStock > 0`.
- Les effets à chance ont une chance > 0 et un cap ou une fréquence claire.
- Les effets de multiplicateur extrême apparaissent correctement dans le score breakdown.
- Les effets qui retirent un dé/module affichent un feedback explicite au joueur.
