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
	RoundEvents.Reset();
	DebugEffectEvents.Reset();
	DebugScoreLines.Reset();
	DebugGoldLines.Reset();
	DebugShopLines.Reset();
	NextDebugSequence = 1;
	NextRoundEventSequence = 1;

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

void UGambitPlayerRoundStateComponent::AddRoundEvent(const FGambitRoundGameplayEvent& Event)
{
	FGambitRoundGameplayEvent StoredEvent = Event;
	StoredEvent.Sequence = NextRoundEventSequence++;
	RoundEvents.Add(StoredEvent);
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

void UGambitPlayerRoundStateComponent::AppendRoundEvents(const TArray<FGambitRoundGameplayEvent>& Events)
{
	for (const FGambitRoundGameplayEvent& Event : Events)
	{
		AddRoundEvent(Event);
	}
}

FGambitScoreModifierContext UGambitPlayerRoundStateComponent::BuildCombinedScoreModifier() const
{
	return RoundConsumableModifier;
}

bool UGambitPlayerRoundStateComponent::HasEventThisRound(const EGambitRoundGameplayEventType EventType) const
{
	return CountEventsThisRound(EventType) > 0;
}

int32 UGambitPlayerRoundStateComponent::CountEventsThisRound(const EGambitRoundGameplayEventType EventType) const
{
	if (EventType == EGambitRoundGameplayEventType::None)
	{
		return RoundEvents.Num();
	}

	return RoundEvents.FilterByPredicate([EventType](const FGambitRoundGameplayEvent& Event)
	{
		return Event.EventType == EventType;
	}).Num();
}

TArray<FGambitRoundGameplayEvent> UGambitPlayerRoundStateComponent::GetRoundEventsByType(
	const EGambitRoundGameplayEventType EventType) const
{
	if (EventType == EGambitRoundGameplayEventType::None)
	{
		return RoundEvents;
	}

	return RoundEvents.FilterByPredicate([EventType](const FGambitRoundGameplayEvent& Event)
	{
		return Event.EventType == EventType;
	});
}

TArray<FGambitRoundGameplayEvent> UGambitPlayerRoundStateComponent::GetRoundEventsBySourceItem(
	const FName SourceItemId) const
{
	return RoundEvents.FilterByPredicate([SourceItemId](const FGambitRoundGameplayEvent& Event)
	{
		return Event.SourceItemId == SourceItemId;
	});
}

TArray<FGambitRoundGameplayEvent> UGambitPlayerRoundStateComponent::GetRoundEventsByEffect(
	const FName EffectId) const
{
	return RoundEvents.FilterByPredicate([EffectId](const FGambitRoundGameplayEvent& Event)
	{
		return Event.EffectId == EffectId;
	});
}

TArray<FGambitRoundGameplayEvent> UGambitPlayerRoundStateComponent::GetRoundEventsByTargetPlayer(
	const int32 TargetPlayerId) const
{
	return RoundEvents.FilterByPredicate([TargetPlayerId](const FGambitRoundGameplayEvent& Event)
	{
		return Event.TargetPlayerId == TargetPlayerId;
	});
}
