#include "Items/Modules/GambitModuleDefinition.h"

namespace
{
	bool IsModuleDefinitionNeutralScoreModifier(const FGambitScoreModifierContext& Modifier)
	{
		return FMath::IsNearlyZero(Modifier.AdditiveBonus)
			&& FMath::IsNearlyZero(Modifier.DiceContributionMultiplierBonus)
			&& FMath::IsNearlyEqual(Modifier.Multiplier, 1.0f)
			&& Modifier.ScoreCap <= 0.0f
			&& Modifier.DiminishingThreshold <= 0.0f
			&& FMath::IsNearlyEqual(Modifier.DiminishingFactor, 1.0f);
	}
}

UGambitModuleDefinition::UGambitModuleDefinition()
{
	ItemType = EGambitItemType::Module;
	ItemTypeId = TEXT("item.module");
}

bool UGambitModuleDefinition::HasNonNeutralPersistentScoreModifier() const
{
	return !IsModuleDefinitionNeutralScoreModifier(PersistentScoreModifier);
}

bool UGambitModuleDefinition::ShouldApplyLegacyPersistentScoreModifier() const
{
	return HasNonNeutralPersistentScoreModifier() && EffectDefinitions.Num() == 0;
}
