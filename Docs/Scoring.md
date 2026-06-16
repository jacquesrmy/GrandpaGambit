# Scoring

## Combination base scores

Dice combination base scores are configured in Project Settings under:

`Game > GrandpaGambit > Scoring > Combination Base Scores`

The default values preserve the original scoring:

| Combination | Base score |
| --- | ---: |
| High Dice | 5 |
| Pair | 12 |
| Two Pair | 18 |
| Three Of A Kind | 25 |
| Straight Small | 30 |
| Full House | 40 |
| Four Of A Kind | 45 |
| Straight Large | 55 |
| Five Of A Kind | 70 |

`UGambitDiceCombinationEvaluator` reads these values through `UGambitGameBalanceSettings`. If the config table is empty, partial, or contains an invalid negative score, the code falls back to the built-in default for that combination.

## Score modifier merge

`FGambitScoreModifierMath` is the single source of truth for merging `FGambitScoreModifierContext` values.

The merge rule is unchanged:

- additive bonuses are added;
- dice contribution bonuses are added;
- multipliers are multiplied;
- score caps use the lower positive cap, or the only positive cap;
- diminishing thresholds use the lower positive threshold, or the only positive threshold;
- diminishing factors use the lower positive factor, or the only positive factor;
- non-positive merged multipliers and diminishing factors normalize back to `1.0`.

Existing public wrappers, such as `UGambitScoreCalculator::MergeModifiers` and `UGambitEffectResolver::MergeScoreModifiers`, delegate to `FGambitScoreModifierMath` for compatibility.
