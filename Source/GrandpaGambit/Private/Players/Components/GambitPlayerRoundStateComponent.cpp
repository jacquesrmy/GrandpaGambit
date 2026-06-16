#include "Players/Components/GambitPlayerRoundStateComponent.h"

#include "Scoring/Calculators/GambitScoreModifierMath.h"

UGambitPlayerRoundStateComponent::UGambitPlayerRoundStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	RoundConsumableModifier = FGambitScoreModifierMath::MakeNeutral();
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

	RoundConsumableModifier = FGambitScoreModifierMath::MakeNeutral();

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
	RoundConsumableModifier = FGambitScoreModifierMath::Merge(RoundConsumableModifier, Modifier);
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
