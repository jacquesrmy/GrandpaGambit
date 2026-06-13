#include "Scoring/Calculators/GambitScoreCalculator.h"

#include "Core/Settings/GambitGameBalanceSettings.h"

FGambitScoreBreakdown UGambitScoreCalculator::CalculateScore(
	const FGambitDiceCombinationResult& CombinationResult,
	const FGambitScoreModifierContext& ModifierContext) const
{
	FGambitScoreBreakdown Breakdown;
	const int32 ResolvedRawScore = CombinationResult.RawScore != 0
		? CombinationResult.RawScore
		: CombinationResult.BaseCombinationScore + CombinationResult.DiceSum;
	Breakdown.Combination = CombinationResult.Combination;
	Breakdown.BaseCombinationScore = CombinationResult.BaseCombinationScore;
	Breakdown.DiceSum = CombinationResult.DiceSum;
	Breakdown.DiceContributionMultiplierBonus = ModifierContext.DiceContributionMultiplierBonus;
	Breakdown.AdjustedDiceSum = static_cast<float>(CombinationResult.DiceSum) + ModifierContext.DiceContributionMultiplierBonus;
	Breakdown.RawScore = ResolvedRawScore;
	Breakdown.AdditiveBonus = ModifierContext.AdditiveBonus;
	Breakdown.Multiplier = ModifierContext.Multiplier;

	const float BaseScore = static_cast<float>(CombinationResult.BaseCombinationScore) + Breakdown.AdjustedDiceSum;
	const float BeforeMultiplier = BaseScore + ModifierContext.AdditiveBonus;
	Breakdown.ScoreBeforeCap = BeforeMultiplier * FMath::Max(ModifierContext.Multiplier, 0.0f);

	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	const float DiminishingThreshold = ModifierContext.DiminishingThreshold > 0.0f
		? ModifierContext.DiminishingThreshold
		: Settings->DefaultDiminishingThreshold;
	const float DiminishingFactor = (ModifierContext.DiminishingFactor > 0.0f)
		? ModifierContext.DiminishingFactor
		: Settings->DefaultDiminishingFactor;

	float ScoreAfterDiminishing = Breakdown.ScoreBeforeCap;
	if (DiminishingThreshold > 0.0f && ScoreAfterDiminishing > DiminishingThreshold)
	{
		const float Overflow = ScoreAfterDiminishing - DiminishingThreshold;
		ScoreAfterDiminishing = DiminishingThreshold + Overflow * FMath::Clamp(DiminishingFactor, 0.0f, 1.0f);
	}

	const float ScoreCap = ModifierContext.ScoreCap > 0.0f ? ModifierContext.ScoreCap : Settings->DefaultScoreCap;
	Breakdown.ScoreAfterCap = (ScoreCap > 0.0f) ? FMath::Min(ScoreAfterDiminishing, ScoreCap) : ScoreAfterDiminishing;
	Breakdown.FinalScore = FMath::Max(FMath::RoundToInt(Breakdown.ScoreAfterCap), 0);
	return Breakdown;
}

FGambitScoreModifierContext UGambitScoreCalculator::MergeModifiers(
	const FGambitScoreModifierContext& PersistentModifier,
	const FGambitScoreModifierContext& RoundModifier) const
{
	FGambitScoreModifierContext Result;
	Result.AdditiveBonus = PersistentModifier.AdditiveBonus + RoundModifier.AdditiveBonus;
	Result.DiceContributionMultiplierBonus =
		PersistentModifier.DiceContributionMultiplierBonus + RoundModifier.DiceContributionMultiplierBonus;
	Result.Multiplier = PersistentModifier.Multiplier * RoundModifier.Multiplier;

	if (PersistentModifier.ScoreCap > 0.0f && RoundModifier.ScoreCap > 0.0f)
	{
		Result.ScoreCap = FMath::Min(PersistentModifier.ScoreCap, RoundModifier.ScoreCap);
	}
	else
	{
		Result.ScoreCap = FMath::Max(PersistentModifier.ScoreCap, RoundModifier.ScoreCap);
	}

	if (PersistentModifier.DiminishingThreshold > 0.0f && RoundModifier.DiminishingThreshold > 0.0f)
	{
		Result.DiminishingThreshold = FMath::Min(PersistentModifier.DiminishingThreshold, RoundModifier.DiminishingThreshold);
	}
	else
	{
		Result.DiminishingThreshold = FMath::Max(PersistentModifier.DiminishingThreshold, RoundModifier.DiminishingThreshold);
	}

	if (PersistentModifier.DiminishingFactor > 0.0f && RoundModifier.DiminishingFactor > 0.0f)
	{
		Result.DiminishingFactor = FMath::Min(PersistentModifier.DiminishingFactor, RoundModifier.DiminishingFactor);
	}
	else
	{
		Result.DiminishingFactor = (PersistentModifier.DiminishingFactor > 0.0f)
			? PersistentModifier.DiminishingFactor
			: RoundModifier.DiminishingFactor;
	}

	if (Result.Multiplier <= 0.0f)
	{
		Result.Multiplier = 1.0f;
	}
	if (Result.DiminishingFactor <= 0.0f)
	{
		Result.DiminishingFactor = 1.0f;
	}

	return Result;
}
