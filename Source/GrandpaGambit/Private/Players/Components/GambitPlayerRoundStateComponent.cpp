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
	RoundFeedbackEvents.Reset();
	ScoreBreakdownLines.Reset();
	GoldBreakdownLines.Reset();
	ShopBreakdownLines.Reset();
	NextFeedbackSequence = 1;
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

void UGambitPlayerRoundStateComponent::AddRoundFeedbackEvent(const FGambitRoundFeedbackEvent& Event)
{
	FGambitRoundFeedbackEvent StoredEvent = Event;
	StoredEvent.Sequence = NextFeedbackSequence++;
	RoundFeedbackEvents.Add(StoredEvent);
}

void UGambitPlayerRoundStateComponent::AddScoreBreakdownLine(const FGambitScoreBreakdownLine& Line)
{
	FGambitScoreBreakdownLine StoredLine = Line;
	StoredLine.Sequence = NextFeedbackSequence++;
	ScoreBreakdownLines.Add(StoredLine);
}

void UGambitPlayerRoundStateComponent::AddGoldBreakdownLine(const FGambitGoldBreakdownLine& Line)
{
	FGambitGoldBreakdownLine StoredLine = Line;
	StoredLine.Sequence = NextFeedbackSequence++;
	GoldBreakdownLines.Add(StoredLine);
}

void UGambitPlayerRoundStateComponent::AddShopBreakdownLine(const FGambitShopBreakdownLine& Line)
{
	FGambitShopBreakdownLine StoredLine = Line;
	StoredLine.Sequence = NextFeedbackSequence++;
	ShopBreakdownLines.Add(StoredLine);
}

void UGambitPlayerRoundStateComponent::AppendRoundFeedbackEvents(const TArray<FGambitRoundFeedbackEvent>& Events)
{
	for (const FGambitRoundFeedbackEvent& Event : Events)
	{
		AddRoundFeedbackEvent(Event);
	}
}

void UGambitPlayerRoundStateComponent::AppendScoreBreakdownLines(const TArray<FGambitScoreBreakdownLine>& Lines)
{
	for (const FGambitScoreBreakdownLine& Line : Lines)
	{
		AddScoreBreakdownLine(Line);
	}
}

void UGambitPlayerRoundStateComponent::AppendGoldBreakdownLines(const TArray<FGambitGoldBreakdownLine>& Lines)
{
	for (const FGambitGoldBreakdownLine& Line : Lines)
	{
		AddGoldBreakdownLine(Line);
	}
}

void UGambitPlayerRoundStateComponent::AppendShopBreakdownLines(const TArray<FGambitShopBreakdownLine>& Lines)
{
	for (const FGambitShopBreakdownLine& Line : Lines)
	{
		AddShopBreakdownLine(Line);
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
