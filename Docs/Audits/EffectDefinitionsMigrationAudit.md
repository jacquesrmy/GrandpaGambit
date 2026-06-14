# EffectDefinitions Migration Audit

Final report for the migration of legacy module and consumable score shortcuts to `EffectDefinitions`.

The asset audit and migration were executed through `GambitEffectDefinitionsAuditCommandlet`, which loads and saves Unreal packages through Unreal APIs. No raw `.uasset` text or binary editing was used.

## Final Asset Audit

Command:

```text
UnrealEditor-Cmd.exe GrandpaGambit.uproject -unattended -nop4 -nosplash -NullRHI -NoSound -run=GambitEffectDefinitionsAudit
```

Final result:

| Metric | Result |
| --- | ---: |
| Audited modules | 87 |
| Audited consumables | 8 |
| Legacy non-neutral assets | 0 |
| Legacy-only assets | 0 |
| Mixed legacy + `EffectDefinitions` assets | 0 |
| Assets blocked by missing dedicated mappings | 0 |
| Asset load failures | 0 |

## Migrated Assets

These assets were legacy-only before migration. Each one now references an `EffectDefinitions` asset and has its legacy shortcut reset to a neutral `FGambitScoreModifierContext`.

| Item asset | Previous legacy field | New effect asset | Effect mapping |
| --- | --- | --- | --- |
| `/Game/GrandpaGambit/Data/Items/Consumables/DA_Item_Consumable_Add15` | `ActionScoreModifier.AdditiveBonus = 15.0` | `/Game/GrandpaGambit/Data/Items/Effects/Consumables/DA_Effect_DA_Item_Consumable_Add15_Legacy_AddScore` | `ConsumableUse` + `AddScoreFlat.Amount = 15.0` |
| `/Game/GrandpaGambit/Data/Items/Consumables/DA_Item_Consumable_Mult130` | `ActionScoreModifier.Multiplier = 1.3` | `/Game/GrandpaGambit/Data/Items/Effects/Consumables/DA_Effect_DA_Item_Consumable_Mult130_Legacy_MultiplyScore` | `ConsumableUse` + `MultiplyScore.Multiplier = 1.3` |
| `/Game/GrandpaGambit/Data/Items/Modules/DA_Item_Module_Add10` | `PersistentScoreModifier.AdditiveBonus = 10.0` | `/Game/GrandpaGambit/Data/Items/Effects/Modules/DA_Effect_DA_Item_Module_Add10_Legacy_AddScore` | `ScoreModifier` + `AddScoreFlat.Amount = 10.0` |
| `/Game/GrandpaGambit/Data/Items/Modules/DA_Item_Module_Flat_Adder` | `PersistentScoreModifier.AdditiveBonus = 10.0` | `/Game/GrandpaGambit/Data/Items/Effects/Modules/DA_Effect_DA_Item_Module_Flat_Adder_Legacy_AddScore` | `ScoreModifier` + `AddScoreFlat.Amount = 10.0` |
| `/Game/GrandpaGambit/Data/Items/Modules/DA_Item_Module_Mult120` | `PersistentScoreModifier.Multiplier = 1.2` | `/Game/GrandpaGambit/Data/Items/Effects/Modules/DA_Effect_DA_Item_Module_Mult120_Legacy_MultiplyScore` | `ScoreModifier` + `MultiplyScore.Multiplier = 1.2` |
| `/Game/GrandpaGambit/Data/Items/Modules/DA_Item_Module_Mult_BasicMultiplier` | `PersistentScoreModifier.Multiplier = 1.2` | `/Game/GrandpaGambit/Data/Items/Effects/Modules/DA_Effect_DA_Item_Module_Mult_BasicMultiplier_Legacy_MultiplyScore` | `ScoreModifier` + `MultiplyScore.Multiplier = 1.2` |

## Mapping Coverage

| Legacy field | EffectDefinitions mapping | Status |
| --- | --- | --- |
| `AdditiveBonus` | `AddScoreFlat.Amount` | Available and used by migrated assets |
| `Multiplier` | `MultiplyScore.Multiplier` | Available and used by migrated assets |
| `DiceContributionMultiplierBonus` | Generic `ScoreModifier.ScoreModifier.DiceContributionMultiplierBonus` | Available; no migrated asset needed it |
| `ScoreCap` | Generic `ScoreModifier.ScoreModifier.ScoreCap` | Available through the generic score modifier effect; no migrated asset needed it |
| `DiminishingThreshold` / `DiminishingFactor` | Generic `ScoreModifier.ScoreModifier` fields | Available through the generic score modifier effect; no migrated asset needed it |

## Remaining Legacy C++ Surface

Legacy fields and fallback paths remain intentionally present in this pass, but authored content should now treat `EffectDefinitions` as the source of truth.

| File | Remaining reason |
| --- | --- |
| `Source/GrandpaGambit/Public/Items/Modules/GambitModuleDefinition.h` | Keeps `PersistentScoreModifier` as a deprecated migration-only editor field and exposes helper predicates. |
| `Source/GrandpaGambit/Private/Items/Modules/GambitModuleDefinition.cpp` | Detects non-neutral legacy module values and allows fallback only when `EffectDefinitions` is empty. |
| `Source/GrandpaGambit/Public/Items/Consumables/GambitConsumableDefinition.h` | Keeps `ActionScoreModifier` as a deprecated migration-only editor field and exposes helper predicates. |
| `Source/GrandpaGambit/Private/Items/Consumables/GambitConsumableDefinition.cpp` | Detects non-neutral legacy consumable values and allows fallback only when `EffectDefinitions` is empty. |
| `Source/GrandpaGambit/Private/Items/Effects/GambitEffectResolver.cpp` | Preserves runtime fallback for legacy-only items; mixed assets ignore legacy values. Also routes `ConsumableUse` score effects into temporary modifier channels. |
| `Source/GrandpaGambit/Private/Players/Components/GambitInventoryComponent.cpp` | Preserves module and consumable fallback for legacy-only definitions. |
| `Source/GrandpaGambit/Private/Data/Validation/GambitDataValidation.cpp` | Makes mixed legacy + `EffectDefinitions` a blocking validation error and warns on legacy-only fallback assets. |
| `Source/GrandpaGambit/Private/Data/Validation/GambitEffectDefinitionsAuditCommandlet.cpp` | Provides repeatable audit and optional safe package migration for future assets. |
| `Source/GrandpaGambit/Private/Debug/GambitDebugAutoPlayer.cpp` | Uses `UGambitEffectResolver` when estimating item modifiers so debug choices reflect `EffectDefinitions`. |
| `Source/GrandpaGambit/Private/Debug/GambitMatchDebugComponent.cpp` | Logs legacy values as `None`, `FallbackActive`, or `IgnoredByEffectDefinitions`. |
| `Source/GrandpaGambit/Private/Tests/GambitEffectResolverTests.cpp` | Covers legacy fallback, mixed validation errors, and migrated consumable effect channel parity. |

## Validation Changes

- Mixed `PersistentScoreModifier` + `EffectDefinitions` module definitions are now blocking validation errors.
- Mixed `ActionScoreModifier` + `EffectDefinitions` consumable definitions are now blocking validation errors.
- Legacy-only module and consumable definitions still validate as warnings because runtime fallback intentionally remains.
- Editor-facing tooltips now describe both legacy fields as migration-only and clarify that `EffectDefinitions` are the source of truth for authored content.

## Runtime Parity Notes

- Module `AddScoreFlat` and `MultiplyScore` effects execute on the persistent score modifier path for `ScoreModifier` hooks.
- Consumable `AddScoreFlat`, `MultiplyScore`, `ScoreModifier`, and `MultiplyDiceContribution` effects execute on the temporary score modifier path for `ConsumableUse` hooks, matching the previous `ActionScoreModifier` behavior.
- Fallback execution is still available only for legacy-only items where `EffectDefinitions` is empty.

## Verification

| Check | Result |
| --- | --- |
| Standard Unity build | Passed with `Build.bat GrandpaGambitEditor Win64 Development` |
| Forced Unity build without adaptive exclusions | Passed with `Build.bat GrandpaGambitEditor Win64 Development -ForceUnity -DisableAdaptiveUnity` |
| Non-Unity build | Passed with `Build.bat GrandpaGambitEditor Win64 Development -DisableUnity` |
| `GrandpaGambit.Items` automation suite | Passed, 1/1 |
| `GrandpaGambit.Consumables` automation suite | Passed, 1/1 |
| `GrandpaGambit.Effects` automation suite | Passed, 17/17 |
| Final asset audit | Passed with 0 legacy-only, 0 mixed, 0 blocked, 0 load failures |

## Risks And Recommendations

- Do not remove the legacy fields or fallback code in this pass. They were intentionally preserved while fixing the Unity build.
- The Unity build failure was caused by repeated helper names inside anonymous namespaces that collide when Unreal aggregates `.cpp` files into unity translation units. Those helpers now use file-specific names.
- New authored module and consumable content should use `EffectDefinitions` only. Legacy shortcut fields should remain neutral until the cleanup pass removes them.
- Since the Unity build, non-Unity build, targeted automation suites, and final asset audit all pass, the next pass can remove `PersistentScoreModifier`, `ActionScoreModifier`, and the legacy fallback paths.
- Keep `GambitEffectDefinitionsAuditCommandlet` available until the cleanup pass is complete so any future legacy-only asset can still be detected through Unreal package loading.
