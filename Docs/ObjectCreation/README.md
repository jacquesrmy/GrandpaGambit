# Object Creation Tracking

Ce dossier sert de plan de production pour les DataAssets d'objets de GrandpaGambit. Il ne cree aucun asset Unreal. Il documente quoi creer, dans quel ordre, et quels objets doivent attendre une mecanique, une UI ou un flow specifique.

## Source de verite

- `ObjectMatrix.csv` est la source de suivi rapide. Une ligne = une fiche de creation DataAsset ou concept DataAsset associe.
- `Dice.md`, `Consumables.md` et `Modules.md` donnent une lecture plus claire par famille.
- `EffectsCoverage.md` decrit ce qui est deja couvert par les enums/effects C++ et ce qui manque.
- `CreationBatches.md` donne les lots de creation recommandes.

## Colonnes de suivi

Dans `ObjectMatrix.csv`, les colonnes importantes sont :

- `Progress` : `todo`, puis `asset_created`, `effect_created`, `validated`, `blocked`, selon l'avancement.
- `CreationClass` : `ReadyNow`, `TestCarefully`, `NeedsMechanicUIFlow` ou `Proposal`.
- `Batch` : lot de creation recommande.
- `Id` : identifiant stable a utiliser dans `ItemId`, `DiceId` ou `EffectId`.
- `Type` : `Die`, `DiceItem`, `Module` ou `Consumable`.
- `EffectType` : enum existant quand possible, ou nom de mecanique manquante si l'objet depasse la couverture actuelle.
- `TechnicalRisk` : raison concrete si l'objet depend d'un systeme absent ou fragile.

## Classification de depart

1. Pret a creer maintenant

Objets definitions-only, score plat, multiplicateurs simples, gold simple, rerolls simples, consommables a effet direct, shop discount/cashback simple, shared pool stock simple.

2. Creable mais a tester attentivement

Objets qui utilisent deja les hooks/effect types existants mais demandent de verifier l'ordre d'execution, les conditions dynamiques, les stacks, les caps, les effets chanceux ou les interactions avec le scoring.

3. Necessite encore une mecanique/UI/flow specifique

Objets qui demandent timer par joueur, placement spatial autour de la table, boutique centrale, encheres, bluff/contestation, ciblage visuel de des adverses, copie du meilleur objet adverse, etat cache/revele, reservation d'offre ou moteur d'effets aleatoires legende.

## Convention d'IDs

- Des : `dice.<slug>`
- Wrapper boutique de de : `item.dice.<slug>`
- Consommables : `consumable.<slug>`
- Modules : `module.<category>.<slug>`
- Effets : `effect.<source_slug>.<hook_or_behavior>`

Les IDs restent en ASCII, minuscules, avec `_` pour les mots composes et `.` pour le domaine. Les noms affiches peuvent rester en francais avec accents dans `DisplayName`.

## Workflow conseille par lot

1. Creer les DataAssets de definitions du lot.
2. Renseigner `ItemId` / `DiceId`, rarete, tags, couts, descriptions et shared pool.
3. Creer ou lier les `UGambitItemEffectDefinition` necessaires.
4. Ajouter les entrees de catalogues et loot tables.
5. Lancer la data validation editor.
6. Jouer un round debug avec au moins deux joueurs locaux.
7. Cocher `Progress` dans `ObjectMatrix.csv`.

## Remarques de design

- Les objets a multiplicateurs extremes sont conserves dans les lots tardifs. Ils risquent de masquer la lisibilite du scoring si les caps/moteurs de breakdown ne sont pas robustes.
- Les objets de sabotage sont limites en priorite. Le pilier du projet parle de negative interaction limitee ; ces objets doivent rester rares, lisibles et defendables.
- Les objets de placement autour de la table sont tres bons pour l'identite du jeu, mais ils doivent attendre une representation claire des positions de des.
- Proposition ajoutee : `module.chance.four_leaf_clover`, un module trefle a quatre feuilles qui gagne `+0.1x` multiplicateur permanent ou de round chaque fois qu'un effet de chance se declenche. Il est interessant mais depend d'un compteur fiable d'effets `ChancePercentage`.
