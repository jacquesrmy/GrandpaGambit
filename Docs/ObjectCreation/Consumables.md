# Consumables DataAsset Plan

Les consommables utilisent `UGambitConsumableDefinition` comme items à usage unique : l'utilisation passe par `ConsumableUse`, puis le slot est retiré de l'inventaire. Les effets retardés passent par `Ranking`, `Reward` ou un état temporaire.

| Id | Nom | Rareté | Coût | Classe | Effet | Cible | Risque |
|---|---|---:|---:|---|---|---|---|
| `consumable.golden_reroll` | Relance Dorée | Common | 3 | Validated | Relance un dé, +3 gold si valeur augmente | SelectedDie | Comparaison avant/après supportée |
| `consumable.lock` | Verrou | Common | 2 | Validated | `LockDie` | SelectedDie | Test ciblé B3 |
| `consumable.polish` | Polissage | Uncommon | 4 | Validated | `TransformDiceForRound` en `dice.silver` | SelectedDie standard | Reset round suivant validé |
| `consumable.injection` | Injection | Common | 3 | Validated | `ModifyDieValue +2` | SelectedDie | Pas de clamp forcé à 6 |
| `consumable.minor_cheat` | Triche Mineure | Rare | 7 | NeedsMechanicUIFlow | Choisir face avant lancer | SelectedDie | UI pre-roll manquante |
| `consumable.bribe` | Pot-de-vin | Uncommon | 5 | Validated | Paie 5 gold, adversaire score x0.9 | TargetOpponent | Coût source + malus cible testés |
| `consumable.fake_face` | Fausse Face | Uncommon | 5 | Blocked | Remplace une face par 6 pour le round | SelectedDie | Non créé : remplacement/restauration temporaire de face manquant |
| `consumable.copycat_consumable` | Copie Conforme | Rare | 6 | NeedsMechanicUIFlow | Copie consommable utilisé ce round | Source | Journal d'utilisation manquant |
| `consumable.stress_hit` | Coup de Stress | Rare | 6 | NeedsMechanicUIFlow | -5 secondes prochain tour pour tous | AllPlayers | Timer manquant |
| `consumable.tax_audit` | Audit Fiscal | Uncommon | 5 | ReadyNow | Joueur le plus riche perd 20% gold | RichestPlayer | Percent spend et égalités à tester |
| `consumable.investment` | Investissement | Uncommon | 5 | ReadyNow | -10 gold puis +3 gold pendant 5 rounds | Source | Recurring income existant |
| `consumable.risky_contract` | Contrat Risqué | Rare | 8 | TestCarefully | x2 ce round, dernier perd un module | Source | Suppression module aléatoire manquante |

## A surveiller

- `Triche Mineure` et `Tricheur Professionnel` devraient partager une UI de choix de face.
- `Copie Conforme` doit attendre un event log de round, sinon l'effet sera fragile.
- `Contrat Risqué` est fort mais punitif. Il doit afficher clairement le risque avant activation.
