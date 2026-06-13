# Dice DataAsset Plan

Les dés de la liste demandent deux familles d'assets :

- `UGambitDiceDefinition` pour les faces, tags, règles runtime et effets de dé.
- `UGambitDiceItemDefinition` pour les vendre en boutique via `GrantedDiceDefinition`.

Le coût appartient au wrapper `item.dice.*`, pas au `dice.*`. Les dés rares et certains dés à forte tension sont proposés en shared pool.

## Dés à créer

| Id | Nom | Rareté | Faces | Classe | Effet / hook principal | Validation |
|---|---|---:|---|---|---|---|
| `dice.standard` | Dé Standard | Common | 1/2/3/4/5/6 | ReadyNow | Aucun | Dé de départ, gratuit |
| `dice.heavy` | Dé Lourd | Common | 2/3/3/4/4/5 | ReadyNow | Aucun | Vérifier absence de 6 |
| `dice.risky` | Dé Risqué | Common | 0/0/3/6/6/9 | ReadyNow | Aucun | Tester 0 et valeurs >6 |
| `dice.even` | Dé Pair | Common | 2/2/4/4/6/6 | ReadyNow | `PostScoreCalculation / AddScoreFlat +2` | Appliquer une fois par dé |
| `dice.odd` | Dé Impair | Common | 1/1/3/3/5/5 | ReadyNow | `Reward / AddGold +1` | Respecter le cap gold |
| `dice.small` | Dé Petit | Common | 1/1/1/2/2/3 | ReadyNow | `DieValueEquals 1 -> +5 score` | Tester plusieurs 1 |
| `dice.big` | Dé Gros | Common | 4/4/5/5/6/6 | ReadyNow | Aucun | Late game fiable |
| `dice.golden` | Dé Doré | Uncommon | 1/2/3/4/5/6 | Validated | Gold = valeur si verrouillé | `gardé` implémenté comme `bLocked` |
| `dice.silver` | Dé Argenté | Uncommon | 1/2/3/4/5/6 | Validated | Score par gold, cap +100 | Scalaire dynamique par gold supporté |
| `dice.red` | Dé Rouge | Uncommon | 1/2/3/4/5/6 | Validated | `>=5 -> +20 score` | Test ciblé B3 |
| `dice.blue` | Dé Bleu | Uncommon | 1/2/3/4/5/6 | Validated | +10 par autre dé bleu | Count par tag de dé supporté |
| `dice.green` | Dé Vert | Uncommon | 1/2/3/4/5/6 | Validated | `<=3 -> +1 reroll` | Test ciblé B3 |
| `dice.purple` | Dé Violet | Uncommon | 1/2/3/4/5/6 | Validated | Matching value -> x1.5 | Condition dynamique valeur source supportée |
| `dice.explosive` | Dé Explosif | Uncommon | 1/2/3/4/5/6 | NeedsMechanicUIFlow | 6 relance/explose | Effet récursif avec cap manquant |
| `dice.cracked` | Dé Fissuré | Uncommon | 1/2/3/4/5/6 | TestCarefully | 6 -> x2, 10% destruction | Vérifier suppression permanente |
| `dice.mirror` | Dé Miroir | Uncommon | 1/2/3/4/5/6 | NeedsMechanicUIFlow | Copie voisin gauche | Target adjacent manquant |
| `dice.magnet` | Dé Aimant | Uncommon | 1/2/3/4/5/6 | NeedsMechanicUIFlow | Voisins +1 max 6 | Placement/adjacence manquants |
| `dice.vampire` | Dé Vampire | Uncommon | 1/2/3/4/5/6 | NeedsMechanicUIFlow | 6 vole score à tous | Multi-target adverses manquant |
| `dice.bank` | Dé Banque | Uncommon | 1/2/3/4/5/6 | TestCarefully | Valeur < moyenne -> +2 gold | Condition moyenne manquante |
| `dice.royal` | Dé Royal | Rare | 3/4/5/6/7/8 | ReadyNow | Aucun | Tester combos avec 7/8 |
| `dice.cursed` | Dé Maudit | Rare | -3/-2/0/6/9/12 | ReadyNow | Aucun | Tester score brut négatif |
| `dice.lucky` | Dé Chanceux | Rare | 1/2/3/4/5/6 | TestCarefully | 6 -> consommable random | Pool random manquant |
| `dice.critical` | Dé Critique | Rare | 1/2/3/4/5/6 | TestCarefully | 6 -> x5 propre score | Per-die multiplier manquant |
| `dice.berserk` | Dé Berserk | Rare | 1/2/3/4/5/6 | NeedsMechanicUIFlow | Face max persistante +1 | Mutation par instance manquante |
| `dice.frozen` | Dé Gelé | Rare | 1/2/3/4/5/6 | TestCarefully | Verrouillage gratuit | Peut être inutile si lock gratuit |
| `dice.ghost` | Dé Fantôme | Rare | 0/0/0/6/6/6 | ReadyNow | Ne compte pas somme, déclenche valeur | Flags déjà présents |
| `dice.double` | Dé Double | Rare | 1/2/3/4/5/6 | ReadyNow | ComboContributionCount=2 | Tester evaluator |
| `dice.triple` | Dé Triple | Rare | 1/2/3/4/5/6 | ReadyNow | ComboContributionCount=3, score brut 0 | Tester séparation combo/score |
| `dice.chrono` | Dé Chrono | Rare | 1/2/3/4/5/6 | NeedsMechanicUIFlow | +10 score/sec restante | Timer manquant |
| `dice.echo` | Dé Écho | Rare | 1/2/3/4/5/6 | NeedsMechanicUIFlow | Copie dernier effet | Filtrage et anti-boucle manquants |
| `dice.cosmic` | Dé Cosmique | Epic | 1/2/3/4/5/6 | NeedsMechanicUIFlow | Face random gagne x2 permanent | Mutation runtime manquante |
| `dice.infinite` | Dé Infini | Legendary | 1/2/3/4/5/6 | NeedsMechanicUIFlow | Même valeur round précédent -> x cumulatif | State cumulatif manquant |
| `dice.god` | Dé Dieu | Legendary | 6/6/6/6/6/6 | ReadyNow | Non relançable | Simple et fort |
| `dice.chaos` | Dé Chaos | Epic | random -10..20 | NeedsMechanicUIFlow | Faces changent chaque round | Génération runtime manquante |
| `dice.black_market` | Dé Marché Noir | Epic | 1/2/3/4/5/6 | TestCarefully | x3, perd 25% gold | Spend percent manquant |
| `dice.copycat` | Dé Copieur | Epic | 1/2/3/4/5/6 | NeedsMechanicUIFlow | Copie meilleur dé adverse | Ciblage/copie inventaire manquants |
| `dice.lord` | Dé Seigneur | Legendary | 1/2/3/4/5/6 | TestCarefully | Même résultat que lui -> x2 | Per-die multiplier manquant |
| `dice.jackpot` | Dé Jackpot | Legendary | 1/1/1/6/6/6 | ReadyNow | Tous les dés 1/6 -> x10 | Condition all-dice à ajouter |

## Wrappers DiceItem

Chaque `dice.*` a une ligne `item.dice.*` dans `ObjectMatrix.csv`.

Règle de création :

- `ItemType = Dice`
- `GrantedDiceDefinition = dice.<slug>`
- `Cost` selon la matrice
- `bUsesSharedPool = true` surtout pour rares, épiques, légendaires et dés à tension forte
- pas d'`EffectDefinition` requise pour le simple grant

## Remarques de design

- Le couple `dice.ghost`, `dice.double`, `dice.triple` est très utile pour tester la séparation score brut / contribution combo.
- Les dés positionnels (`mirror`, `magnet`) doivent attendre une représentation claire de l'ordre des dés.
- Les dés persistants (`berserk`, `cosmic`, `infinite`) ne doivent jamais muter le DataAsset statique. Ils demandent un état runtime par instance.
