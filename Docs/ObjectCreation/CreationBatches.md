# Creation Batches

Ce plan crée d'abord les assets sûrs, puis les synergies testables, puis les systèmes qui demandent de nouveaux flows.

## Résumé

| Lot | Objectif | Lignes CSV | Statut |
|---|---|---:|---|
| B1 | Dés de base et wrappers simples | 14 | [x] validé |
| B2 | Modules score/combo/build fondamentaux | 54 | [x] valide - 52 crees/valides, 2 reclasses |
| B3 | Dés peu communs simples et consommables directs | 17 | [x] valide - 16 crees/valides, 1 bloque |
| B4 | Économie, reroll, shop simple, shared pool simple | 44 | [x] validé périmètre supporté - 33 créés/validés, 11 reclassés |
| B5 | Dés rares/épiques testables et gros chiffres contrôlés | 42 | [x] validé partiel |
| B6 | Interaction, défense et risque modéré | 37 | [ ] à créer |
| B7 | Position table, choix joueur, bluff | 17 | [ ] bloqué mécanique/UI |
| B8 | Temps, boutique centrale, enchères, marché avancé | 18 | [ ] bloqué mécanique/UI |
| B9 | Persistants, copie, chaos, légendaires avancés | 31 | [ ] bloqué systèmes avancés |

## B1 - Fondations dés et DiceItem

À créer :

- `dice.standard`, `dice.heavy`, `dice.risky`, `dice.even`, `dice.odd`, `dice.small`, `dice.big`
- `item.dice.standard`, `item.dice.heavy`, `item.dice.risky`, `item.dice.even`, `item.dice.odd`, `item.dice.small`, `item.dice.big`

Checklist :

- [x] Créer les 7 `UGambitDiceDefinition`.
- [x] Créer les 7 `UGambitDiceItemDefinition`.
- [x] Ajouter les entrées aux catalogues.
- [x] Lancer data validation.
- [x] Tester roll de 4 dés avec valeurs 0, 9 et dés sans 6.
- [x] Mettre `Progress=validated` dans `ObjectMatrix.csv`.

## B2 - Modules de scoring fondamentaux

Inclut score plat, multiplicateurs simples, combos, rareté simple et builds pair/impair/low/high.

Objectif : valider le pipeline `CombinationEvaluation -> ScoreModifier -> ScoreBreakdown`.

Statut de cette passe : B2 finalise. Les modules supportes proprement sont crees et valides, y compris les multiplicateurs par contribution de de. Deux modules restent volontairement non crees et reclasses dans `ObjectMatrix.csv`.

Crees precedemment :
`module.flat.adder`, `module.mult.basic_multiplier`, `module.mult.sixth_sense`,
`module.combo.two_pair`, `module.combo.royal_square`, `module.combo.five_identical`,
`module.combo.full_house`, `module.combo.double_six`, `module.combo.triple_six`.

Crees dans la passe "suite B2" :
`module.flat.small_bonus`, `module.flat.big_bonus`, `module.flat.even_counter`,
`module.flat.odd_counter`, `module.flat.clean_table`, `module.flat.bad_start`,
`module.flat.stable_hand`, `module.flat.guaranteed_minimum`, `module.flat.presence_bonus`,
`module.mult.double_even`, `module.mult.high_voltage`, `module.mult.precision`,
`module.mult.relentless`, `module.mult.calculated_move`, `module.combo.paid_pair`,
`module.combo.brutal_triple`, `module.combo.short_straight`, `module.combo.long_straight`,
`module.combo.bad_hand`, `module.build_even.even_purity`, `module.build_even.two_by_two`,
`module.build_low.ones_army`, `module.build_low.micro_combo`, `module.build_high.big_arms`,
`module.build_high.high_value`.

Crees dans la passe finale B2 :
`module.flat.collector`, `module.flat.monomaniac`, `module.mult.full_table`,
`module.combo.symmetry`, `module.combo.all_or_nothing`, `module.rarity.common_fan`,
`module.rarity.rare_collection`, `module.rarity.luxury`, `module.rarity.poor_but_proud`,
`module.rarity.relics`, `module.rarity.perfect_balance`, `module.build_even.even_king`,
`module.build_odd.odd_king`, `module.build_odd.sacred_five`,
`module.build_low.small_but_strong`, `module.build_low.low_roller`,
`module.build_high.six_domination`, `module.build_high.big_dice_king`.

Non crees volontairement :
`module.flat.last_hope` est `NeedsMechanicUIFlow` car le projet ne suit pas encore un vrai ordre de roll/reroll permettant d'identifier le dernier de lance.
`module.rarity.snob` est `blocked` car il demande un ciblage par rarete au niveau de chaque de, la mise a zero de contribution des des communs, et un breakdown/UI explicite.

Checklist :

- [x] Créer d'abord les modules `PersistentScoreModifier` sans conditions pour le sous-ensemble safe.
- [x] Créer les effets conditionnels simples par valeur/combinaison pour le sous-ensemble safe.
- [x] Ajouter les petites conditions manquantes les plus rentables : parite, all-dice range, raw score threshold.
- [x] Tester avec 2 et 4 joueurs locaux en smoke match AI headless.
- [ ] Tester 3 joueurs locaux en QA jouee manuelle.
- [x] Verifier que les multipliers restent lisibles dans le breakdown pour le smoke test minimal.
- [x] Ajouter les scalaires simples : amount per matching die, owned die, pair count et matching pair count.
- [x] Ajouter les compteurs d'inventaire B2 : types de des distincts, des identiques, des par rarete, items/modules par rarete, set de raretes.
- [x] Ajouter les conditions B2 : palindrome, all dice used by selected combination, else branche via condition inversee.
- [x] Finaliser les per-die multipliers via `MultiplyDiceContribution` et `DiceContributionMultiplierBonus`.
- [x] Ajouter `MultiplierDeltaPerMatchingDie` pour `module.build_high.six_domination`.
- [x] Lancer `GrandpaGambitEditor Win64 Development`.
- [x] Lancer `DataValidation -projectonly`.
- [x] Lancer `Automation RunTests GrandpaGambit`.
- [x] Lancer le smoke scoring representatif : pairs, plusieurs 5, raretes, main symetrique, full table, low build, high build >6.

## B1+B2 Global Verification

Date : 2026-06-04.

Resultat : B1+B2 est considere complet pour passer a B3, hors objets explicitement reclasses/bloques dans `ObjectMatrix.csv`.

Synthese :

- [x] Audit assets/catalogues/loot/shared pool : OK, `Saved/Automation/B1B2GlobalAudit.json`, 66 objets B1+B2 validated couverts, 0 erreur, 0 warning.
- [x] `GrandpaGambitEditor Win64 Development` : OK.
- [x] `DataValidation -projectonly` : OK, 1787 assets valides, 0 erreur, 0 warning.
- [x] `Automation RunTests GrandpaGambit` : OK, 27/27 success, 0 warning, 0 failure.
- [x] Smoke match local 2 joueurs : OK, `Saved/Automation/SmokeMatch2.log`, 8 rounds, `Phase=None`, achats Module et DiceItem, score breakdown lisible.
- [x] Smoke match local 4 joueurs : OK, `Saved/Automation/SmokeMatch4.log`, 8 rounds, `Phase=None`, achats Module et DiceItem, score breakdown lisible. Observation non bloquante : le debug AI tente parfois un achat refuse par l'inventaire, puis le match continue normalement.
- [x] Smoke shop/shared pool : OK via tests `GrandpaGambit.Shop.*`, audit loot/shared pool, et smoke match. Le shared pool actif ne contient que `module.combo.five_identical`, `module.rarity.relics`, `module.rarity.perfect_balance`.
- [x] Smoke scoring : OK via `GrandpaGambit.B2.SmokeScoringRepresentativeHands` et logs de match, couvrant pair, full house, several/five-like values, low build, high build, rarete mixte, branches inversees et per-die contribution multipliers.
- [x] `ObjectMatrix.csv` verifie : B1/B2 validated coherents ; `module.flat.last_hope` reste `NeedsMechanicUIFlow`/blocked ; `module.rarity.snob` reste blocked/reclasse.
- [x] References legacy retirees des tables actives : le loot table et le shared pool ne referencent plus les anciens assets prototype hors matrice.

## B3 - Consommables directs et dés peu communs simples

Inclut :

- Consommables : Relance Dorée, Verrou, Polissage, Injection, Pot-de-vin, Fausse Face.
- Dés simples : Rouge, Vert, Doré, Argenté, Bleu, Violet.

Statut de cette passe : B3 est valide pour le sous-ensemble supporte proprement. Les 6 des, les 6 wrappers `item.dice.*` et les 5 consommables directs autorises ont ete crees, ajoutes aux catalogues et au loot table. `consumable.fake_face` reste volontairement bloque car le projet ne possede pas encore de remplacement temporaire d'une face de de avec restauration propre au round suivant.

Crees :
`dice.red`, `dice.green`, `dice.golden`, `dice.silver`, `dice.blue`, `dice.purple`,
`item.dice.red`, `item.dice.green`, `item.dice.golden`, `item.dice.silver`, `item.dice.blue`, `item.dice.purple`,
`consumable.golden_reroll`, `consumable.lock`, `consumable.polish`, `consumable.injection`, `consumable.bribe`.

Non crees volontairement :
`consumable.fake_face` est `blocked` : `SetDieValue`/`ModifyDieValue` ne remplacent pas une face authorable du de et ne garantissent pas une restauration propre de la table de faces au round suivant.
`consumable.minor_cheat`, `consumable.copycat_consumable` et `consumable.stress_hit` ne sont pas crees dans B3 par demande explicite : ils attendent respectivement UI pre-roll, journal d'utilisation/reaction, ou timer/flow avance.

Checklist :

- [x] Implementer ou confirmer `SelectedDie` target rule.
- [x] Confirmer `TargetOpponent` pour consommables adverses.
- [x] Tester `ConsumableUse` en Action phase.
- [x] Tester transformation temporaire en `dice.silver`.
- [x] Tester `ModifyDieValue` sans clamp force a 6 pour Injection.
- [x] Tester `LockDie` sur de selectionne.
- [x] Tester `RerollDie` depuis consommable avec comparaison avant/apres pour Relance Doree.
- [x] Tester ciblage adversaire pour Pot-de-vin.
- [x] Garder les uses per round / slots inventory via le composant inventaire.

## B4 - Économie, reroll, shop et shared pool simple

Objectif : rendre la boutique intéressante sans UI avancée.

Statut de cette passe : B4 est validé sur le périmètre supporté avant B6. Les mécaniques B4 ajoutées restent petites et data-driven : condition moyenne de dés, scalaires de score par gold courant et par tranche de gold, conditions lowest/highest gold, dernier rang dynamique 2-4 joueurs, `PostPurchase` sur module acquis, suivi des deltas de relance, offre boutique gratuite aléatoire, et compteurs globaux d'achats par type/rareté.

Créés précédemment :
`module.economy.piggy_bank`, `module.economy.interests`, `module.economy.cheapskate`,
`module.economy.cashback`, `module.economy.bargaining`, `module.economy.debt`,
`module.economy.victory_bonus`, `module.reroll.free_reroll`,
`module.reroll.profitable_reroll`, `module.reroll.dangerous_reroll`,
`module.reroll.greed_roll`, `consumable.investment`.

Créés dans la passe completion B4 :
`dice.bank`, `item.dice.bank`, `module.build_even.pair_banking`,
`module.build_odd.golden_odd`, `module.economy.loan_shark`,
`module.economy.private_bank`, `module.economy.capitalism`,
`module.economy.outsider_bonus`, `module.economy.minimum_wage`,
`module.economy.coupon`, `module.economy.inflation`,
`module.economy.minor_jackpot`, `module.rarity.niche_market`,
`module.shared.golden_pool`, `module.shared.shortage`,
`module.shared.stockout`, `module.reroll.one_more`,
`module.reroll.master`, `module.reroll.determination`,
`module.reroll.frustration`, `module.big.deluge`.

Non créés volontairement et reclassés dans `ObjectMatrix.csv` :
`module.economy.big_spender` attend une détection fiable "dépense toutes tes pièces" et un buff delayed next-round.
`module.reroll.paid_reroll` et `module.risk.addiction` attendent une vraie commande/action de relance payante.
`module.risk.shaking_hand` attend un override robuste de relance de dés verrouillés.
`module.reroll.free_lock` est bloqué tant qu'un coût de verrouillage n'existe pas.
`module.reroll.last_chance`, `module.reroll.cursed_reroll`, `module.shared.golden_monopoly`, `module.risk.tax`, `module.build_odd.odd_chaos` et `module.legendary.supreme_banker` restent hors périmètre supporté B4 car ils demandent un flow, un ciblage, un design ou un cap plus avancé.

Inclut :

- Tirelire, Intérêts, Radin, Cashback, Marchandage, Dette, Usurier, Salaire Minimum, Prime de Victoire.
- Relance Gratuite, Relance Rentable, Greed Roll, Relance Dangereuse.
- Pool Doré, Pénurie, Rupture de Stock.
- Investissement.

Checklist :

- [x] Valider gold cap, dette, interêts et recurring income.
- [x] Valider `PrePriceResolve` / `PostPurchase`.
- [x] Valider stock shared pool, achat partagé, stock dynamique et compteurs globaux d'achat.
- [x] Tester skip shop et achats multiples.
- [x] Tester moyenne de dés, gold courant, tranches de gold, lowest gold et dernier rang dynamique 2-4 joueurs.
- [x] Tester coupon à une seule offre gratuite.
- [x] Tester deltas de relance : augmentation, diminution et aucun changement.

## B5 - Dés rares, runtime dice flags et gros chiffres contrôlés

Objectif : tester les pouvoirs de dés sans encore ouvrir les gros systèmes persistants.

Statut de cette passe : B5 est valide pour le sous-ensemble supporte proprement. Les 8 des, les 8 wrappers `item.dice.*` et les 3 modules de gros chiffres controles ont ete crees, ajoutes aux catalogues, au loot table et au shared pool quand necessaire.

Créés :

- Des : `dice.royal`, `dice.cursed`, `dice.ghost`, `dice.double`, `dice.triple`, `dice.god`, `dice.cracked`, `dice.jackpot`.
- Wrappers : `item.dice.royal`, `item.dice.cursed`, `item.dice.ghost`, `item.dice.double`, `item.dice.triple`, `item.dice.god`, `item.dice.cracked`, `item.dice.jackpot`.
- Modules : `module.big.crazy_multiplier`, `module.big.dice_storm`, `module.big.ultimate_jackpot`.

Reclasses / bloques explicitement :

- `dice.lucky` et `item.dice.lucky` : random `GrantConsumable` pool non robuste/data-driven.
- `dice.critical` et `item.dice.critical` : le multiplicateur de son propre score de de n'est pas distingue du score final.
- `dice.black_market` et `item.dice.black_market` : depense en pourcentage du gold courant non supportee.
- `dice.lord` et `item.dice.lord` : multiplicateur par de propre correspondant non clair dans le pipeline.
- `module.big.avalanche` : compteur de seuils de score franchis manquant.
- `module.big.point_machine` : scalaire `OwnedDiceCount * ModuleCount` non supporte proprement.
- `module.big.divine_combo` : condition composite fragile avec 4 des et non modelisee comme condition unique.

Inclut :

- Royal, Maudit, Fantôme, Double, Triple, Dieu.
- Fissuré, Chanceux, Critique, Marché Noir, Seigneur, Jackpot.
- Multiplicateur Fou, Tempête de Dés, Avalanche, Machine à Points, Jackpot Ultime.

Checklist :

- [x] Tester `ComboContributionCount`.
- [x] Tester `bCountsForScoreSum=false`.
- [x] Tester destruction marquée après round.
- [x] Ajouter ou bloquer les per-die multipliers avant de créer Critique/Seigneur.
- [x] Poser des caps de score pour jackpots.

## B6 - Interaction, défense et risque

Objectif : ajouter de la tension sans casser le pilier "limited negative interaction".

Inclut :

- Voleur de Points, Taxe de Luxe, Vengeance, Sang-Froid, Assurance.
- Bouclier, Coffre-Fort, Antivol, Calme, Assurance Score, Protection Rare, Table Stable.
- Pacte Maudit, Sang contre Or, Addiction, Roulette.

Checklist :

- [ ] Définir les sous-types d'effets négatifs.
- [ ] Ajouter target rules leader/richest/all opponents si nécessaire.
- [ ] Tester en 4 joueurs locaux.
- [ ] Vérifier que chaque sabotage a un feedback clair et une défense possible.

## B7 - Position, bluff et choix joueur

Bloqué tant que ces systèmes ne sont pas prêts :

- Ordre et zones de dés sur table.
- Visibilité cachée/exposée.
- UI choix face, rival, pair/impair.
- Bluff et contestation.

Ne pas créer en production avant ces systèmes. On peut créer des prototypes debug seulement si `Progress=blocked` reste visible dans la matrice.

## B8 - Temps et marché avancé

Bloqué tant que ces systèmes ne sont pas prêts :

- Timer par joueur.
- Boutique centrale.
- Enchères.
- Réservation ou verrouillage d'offre.
- Coûts non-gold.

Ce lot doit être traité comme un chantier de flow, pas comme une simple passe DataAsset.

## B9 - Persistants, copie et légendaires avancés

Bloqué tant que ces systèmes ne sont pas prêts :

- État persistant par instance de dé.
- Mutation runtime de faces.
- Copie de module/dé adverse.
- Effets aléatoires avec anti-boucle.
- Multiplicateurs persistants avec caps.
- Event history complet.

Checklist avant démarrage :

- [ ] Score caps et breakdown finalisés.
- [ ] Runtime state par player/die/module validé.
- [ ] Event history disponible.
- [ ] UI explique clairement les effets copiés ou aléatoires.

## Après chaque lot

- [ ] Mettre à jour `Progress` dans `ObjectMatrix.csv`.
- [ ] Ajouter les assets au catalogue approprié.
- [ ] Lancer data validation.
- [ ] Tester au moins un round complet.
- [ ] Noter les problèmes de balance dans `ValidationNotes`.



