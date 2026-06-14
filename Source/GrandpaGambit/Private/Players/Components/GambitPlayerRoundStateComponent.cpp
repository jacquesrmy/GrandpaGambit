#include "Players/Components/GambitPlayerRoundStateComponent.h"

namespace
{
	FGambitScoreModifierContext MergeScoreModifiers(
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
}

UGambitPlayerRoundStateComponent::UGambitPlayerRoundStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	RoundConsumableModifier.Multiplier = 1.0f;
	RoundConsumableModifier.DiminishingFactor = 1.0f;
}

void UGambitPlayerRoundStateComponent::InitializeForMatch()
{
	ResetRoundState();
}

void UGambitPlayerRoundStateComponent::ResetRoundState()
{
	CurrentRoundScore = 0;
	LastScoreBreakdown = FGambitScoreBreakdown();
	DebugEffectEvents.Reset();
	DebugScoreLines.Reset();
	DebugGoldLines.Reset();
	DebugShopLines.Reset();
	NextDebugSequence = 1;

	RoundConsumableModifier = FGambitScoreModifierContext();
	RoundConsumableModifier.Multiplier = 1.0f;
	RoundConsumableModifier.DiminishingFactor = 1.0f;

	OnRoundScoreChanged.Broadcast(CurrentRoundScore);
}

void UGambitPlayerRoundStateComponent::ApplyRoundScore(const FGambitScoreBreakdown& ScoreBreakdown)
{
	LastScoreBreakdown = ScoreBreakdown;
	CurrentRoundScore = ScoreBreakdown.FinalScore;
	OnRoundScoreChanged.Broadcast(CurrentRoundScore);
}

void UGambitPlayerRoundStateComponent::ApplyTemporaryScoreModifier(const FGambitScoreModifierContext& Modifier)
{
	RoundConsumableModifier = MergeScoreModifiers(RoundConsumableModifier, Modifier);
}

void UGambitPlayerRoundStateComponent::AddDebugEffectEvent(const FGambitDebugEffectEvent& Event)
{
	FGambitDebugEffectEvent StoredEvent = Event;
	StoredEvent.Sequence = NextDebugSequence++;
	DebugEffectEvents.Add(StoredEvent);
}

void UGambitPlayerRoundStateComponent::AddDebugScoreLine(const FGambitDebugScoreLine& Line)
{
	FGambitDebugScoreLine StoredLine = Line;
	StoredLine.Sequence = NextDebugSequence++;
	DebugScoreLines.Add(StoredLine);
}

void UGambitPlayerRoundStateComponent::AddDebugGoldLine(const FGambitDebugGoldLine& Line)
{
	FGambitDebugGoldLine StoredLine = Line;
	StoredLine.Sequence = NextDebugSequence++;
	DebugGoldLines.Add(StoredLine);
}

void UGambitPlayerRoundStateComponent::AddDebugShopLine(const FGambitDebugShopLine& Line)
{
	FGambitDebugShopLine StoredLine = Line;
	StoredLine.Sequence = NextDebugSequence++;
	DebugShopLines.Add(StoredLine);
}

void UGambitPlayerRoundStateComponent::AppendDebugEffectEvents(const TArray<FGambitDebugEffectEvent>& Events)
{
	for (const FGambitDebugEffectEvent& Event : Events)
	{
		AddDebugEffectEvent(Event);
	}
}

void UGambitPlayerRoundStateComponent::AppendDebugScoreLines(const TArray<FGambitDebugScoreLine>& Lines)
{
	for (const FGambitDebugScoreLine& Line : Lines)
	{
		AddDebugScoreLine(Line);
	}
}

void UGambitPlayerRoundStateComponent::AppendDebugGoldLines(const TArray<FGambitDebugGoldLine>& Lines)
{
	for (const FGambitDebugGoldLine& Line : Lines)
	{
		AddDebugGoldLine(Line);
	}
}

void UGambitPlayerRoundStateComponent::AppendDebugShopLines(const TArray<FGambitDebugShopLine>& Lines)
{
	for (const FGambitDebugShopLine& Line : Lines)
	{
		AddDebugShopLine(Line);
	}
}

FGambitScoreModifierContext UGambitPlayerRoundStateComponent::BuildCombinedScoreModifier() const
{
	return RoundConsumableModifier;
}
