#include "Items/Consumables/GambitConsumableDefinition.h"

namespace
{
	bool IsNeutralScoreModifier(const FGambitScoreModifierContext& Modifier)
	{
		return FMath::IsNearlyZero(Modifier.AdditiveBonus)
			&& FMath::IsNearlyZero(Modifier.DiceContributionMultiplierBonus)
			&& FMath::IsNearlyEqual(Modifier.Multiplier, 1.0f)
			&& Modifier.ScoreCap <= 0.0f
			&& Modifier.DiminishingThreshold <= 0.0f
			&& FMath::IsNearlyEqual(Modifier.DiminishingFactor, 1.0f);
	}
}

UGambitConsumableDefinition::UGambitConsumableDefinition()
{
	ItemType = EGambitItemType::Consumable;
	ItemTypeId = TEXT("item.consumable");
	UsablePhases.Add(EGambitRoundPhase::Action);
}

bool UGambitConsumableDefinition::CanBeUsedDuringPhase(const EGambitRoundPhase Phase) const
{
	return Phase != EGambitRoundPhase::None && UsablePhases.Contains(Phase);
}

bool UGambitConsumableDefinition::HasNonNeutralActionScoreModifier() const
{
	return !IsNeutralScoreModifier(ActionScoreModifier);
}

bool UGambitConsumableDefinition::ShouldApplyLegacyActionScoreModifier() const
{
	return HasNonNeutralActionScoreModifier() && EffectDefinitions.Num() == 0;
}
