# Modules DataAsset Plan

Les modules utilisent `UGambitModuleDefinition`. Les bonus fixes simples et les effets conditionnels doivent passer par `UGambitItemEffectDefinition`.

La fiche complète de chaque module est dans `ObjectMatrix.csv`. Ce fichier sert à lire les familles et les priorités sans parcourir tout le CSV.

## Lecture rapide

| Famille | Lot conseillé | Statut dominant | Notes |
|---|---|---|---|
| Score plat | B2 | ReadyNow | Bons premiers assets pour tester le scoring |
| Multiplicateurs simples | B2 | ReadyNow/TestCarefully | Vérifier ordre add/mult et conditions reroll |
| Combos de dés | B2 | ReadyNow/TestCarefully | Dépend de la richesse de `FGambitDiceCombinationResult` |
| Économie | B4 | ReadyNow/TestCarefully | Très utile pour valider gold, interest, shop |
| Rareté | B2/B4/B6 | TestCarefully | Demande des compteurs par rareté et inventaire |
| Relance | B4 | ReadyNow/TestCarefully | Dépend du tracking avant/après reroll |
| Position autour de la table | B7 | NeedsMechanicUIFlow | Attendre ordre spatial, zones et visibilité |
| Compétitif | B6/B7/B8 | Mixed | Garder rare, lisible et défendable |
| Pools partagés | B4/B8 | Mixed | B4 pour stock simple, B8 pour shop central/enchères |
| Gros chiffres | B5/B9 | TestCarefully/Needs | Demande caps, compteurs et breakdown clair |
| Risque | B4/B6/B9 | Mixed | Souvent punitif, feedback obligatoire |
| Défense | B6/B7 | Mixed | Besoin typage des effets négatifs |
| Builds spécifiques | B2/B4 | ReadyNow/TestCarefully | Bons lots de synergies lisibles |
| Temps | B8 | NeedsMechanicUIFlow | Timer par joueur absent |
| Légendaires | B5/B9 | Needs/Test | Ne pas créer avant caps et états persistants |

## Modules prêts pour les premiers lots

Créer d'abord ces modules pour obtenir une base jouable et testable :

- Score plat : `module.flat.adder`, `module.flat.small_bonus`, `module.flat.big_bonus`, `module.flat.even_counter`, `module.flat.odd_counter`, `module.flat.clean_table`, `module.flat.bad_start`, `module.flat.stable_hand`, `module.flat.guaranteed_minimum`, `module.flat.presence_bonus`.
- Multiplicateurs : `module.mult.basic_multiplier`, `module.mult.sixth_sense`, `module.mult.double_even`, `module.mult.high_voltage`, `module.mult.precision`, `module.mult.relentless`, `module.mult.calculated_move`.
- Combos : `module.combo.two_pair`, `module.combo.brutal_triple`, `module.combo.royal_square`, `module.combo.short_straight`, `module.combo.long_straight`, `module.combo.full_house`, `module.combo.double_six`, `module.combo.triple_six`, `module.combo.bad_hand`.
- Économie : `module.economy.piggy_bank`, `module.economy.interests`, `module.economy.cheapskate`, `module.economy.cashback`, `module.economy.bargaining`, `module.economy.debt`, `module.economy.minimum_wage`, `module.economy.victory_bonus`.
- Relance : `module.reroll.free_reroll`, `module.reroll.profitable_reroll`, `module.reroll.dangerous_reroll`, `module.reroll.greed_roll`.
- Builds : `module.build_even.even_purity`, `module.build_even.pair_banking`, `module.build_odd.golden_odd`, `module.build_low.ones_army`, `module.build_low.micro_combo`, `module.build_high.big_arms`, `module.build_high.high_value`.

## Modules créables mais à tester attentivement

Ces modules sont intéressants rapidement, mais demandent un test ciblé car ils introduisent du comptage dynamique, de l'état retardé ou du scaling :

- Collection et rareté : `module.flat.collector`, `module.flat.monomaniac`, `module.rarity.common_fan`, `module.rarity.rare_collection`, `module.rarity.luxury`, `module.rarity.poor_but_proud`, `module.rarity.niche_market`, `module.rarity.relics`, `module.rarity.perfect_balance`.
- Reroll avancé : `module.reroll.one_more`, `module.reroll.master`, `module.reroll.determination`, `module.reroll.frustration`, `module.reroll.last_chance`, `module.reroll.cursed_reroll`.
- Économie scaling : `module.economy.private_bank`, `module.economy.capitalism`, `module.economy.big_spender`, `module.economy.coupon`, `module.economy.inflation`, `module.economy.loan_shark`, `module.economy.minor_jackpot`, `module.economy.outsider_bonus`.
- Gros chiffres B5 : `module.big.crazy_multiplier`, `module.big.dice_storm`, `module.big.avalanche`, `module.big.point_machine`, `module.big.divine_combo`, `module.big.deluge`.
- Risque/defense : `module.risk.cursed_pact`, `module.risk.addiction`, `module.risk.fragile`, `module.risk.roulette`, `module.risk.shaking_hand`, `module.risk.tax`, `module.risk.six_curse`, `module.defense.false_weak`.

## Modules à bloquer jusqu'à mécanique ou UI dédiée

Ces modules ne devraient pas être créés en DataAssets de production tant que le système support n'est pas conçu :

- Position table : `module.position.left_side`, `module.position.right_side`, `module.position.table_center`, `module.position.formation`, `module.position.triangle`, `module.position.ritual_circle`, `module.position.hidden_face`, `module.position.exposed_die`, `module.position.intimidation`, `module.position.lateral_pressure`.
- Bluff et social : `module.competitive.bluffer`, `module.competitive.accusation`.
- Shop partagé avancé : `module.shared.common_market`, `module.shared.first_arrived`, `module.shared.auctions`, `module.shared.counterfeit`, `module.shared.common_black_market`, `module.shared.market_control`.
- Temps : `module.time.pressure`, `module.time.panic`, `module.time.timed_composure`, `module.time.slow_calculator`, `module.time.blitz`, `module.time.mental_break`.
- Copie et chaos légendaire : `module.legendary.perfect_copy`, `module.legendary.chaos_lord`, `module.legendary.cosmic_casino`, `module.legendary.market_king`, `module.legendary.architect`.
- Persistants lourds : `module.big.escalation`, `module.big.infinite_engine`, `module.big.chain_reaction`, `module.big.combo_breaker`, `module.big.exponential`, `module.big.ascension`, `module.big.ceiling_breaker`, `module.legendary.divine_machine`, `module.legendary.infinite_economy`.

## Remarques de design

- `module.competitive.anti_leader` est à reformuler. "Tes dés contre lui gagnent +1" n'a pas de sens clair si la résolution n'est pas duel par duel.
- `module.risk.blood_for_gold` ressemble plus à un consommable ou à un module à effet d'achat immédiat. Il est viable, mais il faut décider s'il prend un slot module durablement.
- `module.time.mental_break` ressemble aussi plus à un consommable. Comme module, il demande un état "once per match".
- Les modules défensifs ont besoin d'un typage des effets négatifs : vol gold, vol score, modification de dé, destruction, sabotage shop.
- Les gros multiplicateurs doivent attendre un score breakdown lisible et des caps configurables.

## Proposition ajoutée

`module.chance.four_leaf_clover` : Trèfle à Quatre Feuilles.

Effet proposé : chaque fois qu'un effet de chance du joueur réussit, le module gagne `+0.1x` multiplicateur. Deux variantes possibles :

- Variante lisible : bonus reset à la fin du round, cap `+1.0x` par round.
- Variante build long terme : bonus permanent, cap global `+2.0x` ou `+3.0x`.

Je recommande la variante reset par round pour commencer. Elle complète `Dé Chanceux`, `Jackpot Mineur`, `Roulette`, `Chaos Impair` et `Réaction en Chaîne` sans créer un snowball permanent trop tôt.
