# Effects Coverage

Cette page relie la liste d'objets aux capacités C++ déjà présentes dans le projet. Elle sert à éviter de créer des DataAssets qui semblent configurables mais qui ne peuvent pas encore s'exécuter correctement.

## Déjà couvert par les enums existants

Hooks utiles déjà disponibles :

- Round : `RoundStart`, `PreRoll`, `PostRoll`, `PreReroll`, `PostReroll`, `RoundEnd`
- Score : `PreCombinationEvaluation`, `PostCombinationEvaluation`, `PreScoreCalculation`, `ScoreModifier`, `PostScoreCalculation`
- Économie/ranking : `Reward`, `Ranking`
- Boutique : `PreShopGenerate`, `PostShopGenerate`, `PrePriceResolve`, `PostPriceResolve`, `PrePurchase`, `PostPurchase`, `ShopSkipped`
- Consommables : `ConsumableUse`

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
- Target rules de base : selected die, source die, all own dice, random own die, best own die.
- Breakdown combo enrichi restant : les indexes et le flag all-dice-used existent; il reste surtout le feedback UI.

## À ajouter pour les objets B6

Ces ajouts concernent surtout l'interaction locale :

- Target rules adverses : leading player, richest player, all opponents, left player, right player, opposite player, selected opponent die.
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

## TargetRule recommandé

Utiliser des `TargetRuleId` stables, même avant que tous les resolvers existent :

- `target.self`
- `target.selected_die`
- `target.source_die`
- `target.all_own_dice`
- `target.random_own_die`
- `target.best_own_die`
- `target.target_opponent`
- `target.selected_opponent_die`
- `target.leading_player`
- `target.richest_player`
- `target.all_opponents`
- `target.left_player`
- `target.right_player`
- `target.opposite_player`
- `target.shared_pool_item`
- `target.shop_offer`
- `target.all_players`

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
