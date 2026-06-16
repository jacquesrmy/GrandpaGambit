#include "Scoring/Calculators/GambitScoreModifierMath.h"

FGambitScoreModifierContext FGambitScoreModifierMath::MakeNeutral()
{
	return FGambitScoreModifierContext();
}

FGambitScoreModifierContext FGambitScoreModifierMath::Normalize(FGambitScoreModifierContext Modifier)
{
	if (Modifier.Multiplier <= 0.0f)
	{
		Modifier.Multiplier = 1.0f;
	}
	if (Modifier.DiminishingFactor <= 0.0f)
	{
		Modifier.DiminishingFactor = 1.0f;
	}

	return Modifier;
}

bool FGambitScoreModifierMath::IsNeutral(const FGambitScoreModifierContext& Modifier)
{
	return FMath::IsNearlyZero(Modifier.AdditiveBonus)
		&& FMath::IsNearlyZero(Modifier.DiceContributionMultiplierBonus)
		&& FMath::IsNearlyEqual(Modifier.Multiplier, 1.0f)
		&& Modifier.ScoreCap <= 0.0f
		&& Modifier.DiminishingThreshold <= 0.0f
		&& FMath::IsNearlyEqual(Modifier.DiminishingFactor, 1.0f);
}

FGambitScoreModifierContext FGambitScoreModifierMath::Merge(
	const FGambitScoreModifierContext& A,
	const FGambitScoreModifierContext& B)
{
	FGambitScoreModifierContext Result;
	Result.AdditiveBonus = A.AdditiveBonus + B.AdditiveBonus;
	Result.DiceContributionMultiplierBonus = A.DiceContributionMultiplierBonus + B.DiceContributionMultiplierBonus;
	Result.Multiplier = A.Multiplier * B.Multiplier;

	if (A.ScoreCap > 0.0f && B.ScoreCap > 0.0f)
	{
		Result.ScoreCap = FMath::Min(A.ScoreCap, B.ScoreCap);
	}
	else
	{
		Result.ScoreCap = FMath::Max(A.ScoreCap, B.ScoreCap);
	}

	if (A.DiminishingThreshold > 0.0f && B.DiminishingThreshold > 0.0f)
	{
		Result.DiminishingThreshold = FMath::Min(A.DiminishingThreshold, B.DiminishingThreshold);
	}
	else
	{
		Result.DiminishingThreshold = FMath::Max(A.DiminishingThreshold, B.DiminishingThreshold);
	}

	if (A.DiminishingFactor > 0.0f && B.DiminishingFactor > 0.0f)
	{
		Result.DiminishingFactor = FMath::Min(A.DiminishingFactor, B.DiminishingFactor);
	}
	else
	{
		Result.DiminishingFactor = (A.DiminishingFactor > 0.0f) ? A.DiminishingFactor : B.DiminishingFactor;
	}

	return Normalize(Result);
}

FGambitScoreModifierContext FGambitScoreModifierMath::MergeAll(
	TConstArrayView<FGambitScoreModifierContext> Modifiers)
{
	FGambitScoreModifierContext Result = MakeNeutral();
	for (const FGambitScoreModifierContext& Modifier : Modifiers)
	{
		Result = Merge(Result, Modifier);
	}

	return Result;
}
