#include "Players/Components/GambitEconomyComponent.h"

#include "Core/Settings/GambitGameBalanceSettings.h"

namespace
{
	void AddGoldDebugLine(
		TArray<FGambitDebugGoldLine>& Lines,
		const EGambitDebugGoldLineType LineType,
		const FName SourceId,
		const FString& SourceName,
		const int32 RequestedDelta,
		const int32 GoldBefore,
		const int32 GoldAfter,
		const FString& Summary)
	{
		FGambitDebugGoldLine Line;
		Line.LineType = LineType;
		Line.SourceId = SourceId;
		Line.SourceName = SourceName;
		Line.RequestedDelta = RequestedDelta;
		Line.ActualDelta = GoldAfter - GoldBefore;
		Line.GoldBefore = GoldBefore;
		Line.GoldAfter = GoldAfter;
		Line.bClamped = RequestedDelta != Line.ActualDelta;
		Line.Summary = Summary;
		Lines.Add(Line);
	}
}

UGambitEconomyComponent::UGambitEconomyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGambitEconomyComponent::InitializeForMatch()
{
	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	DebtLimit = 0;
	MaxGoldDelta = 0;
	InterestIntervalDelta = 0;
	MaxInterestDelta = 0;
	InterestBonusDelta = 0;
	RecurringGoldIncomes.Reset();
	DebtLimitModifiers.Reset();
	MaxGoldModifiers.Reset();
	InterestIntervalModifiers.Reset();
	MaxInterestModifiers.Reset();
	InterestBonusModifiers.Reset();
	CurrentGold = ClampGold(Settings->StartingGold);
	OnGoldChanged.Broadcast(CurrentGold);
}

int32 UGambitEconomyComponent::AddGold(const int32 DeltaGold)
{
	CurrentGold = ClampGold(CurrentGold + DeltaGold);
	OnGoldChanged.Broadcast(CurrentGold);
	return CurrentGold;
}

bool UGambitEconomyComponent::SpendGold(const int32 Cost)
{
	if (!CanSpendGold(Cost))
	{
		return false;
	}

	CurrentGold = ClampGold(CurrentGold - Cost);
	OnGoldChanged.Broadcast(CurrentGold);
	return true;
}

bool UGambitEconomyComponent::CanSpendGold(const int32 Cost) const
{
	return Cost >= 0 && CurrentGold - Cost >= GetEffectiveMinimumGold();
}

int32 UGambitEconomyComponent::ApplyRoundEconomy(const int32 BaseGoldReward)
{
	TArray<FGambitDebugGoldLine> IgnoredLines;
	return ApplyRoundEconomyDetailed(BaseGoldReward, IgnoredLines);
}

int32 UGambitEconomyComponent::ApplyRoundEconomyDetailed(
	const int32 BaseGoldReward,
	TArray<FGambitDebugGoldLine>& OutGoldLines)
{
	const int32 BaseBefore = CurrentGold;
	AddGold(BaseGoldReward);
	AddGoldDebugLine(
		OutGoldLines,
		EGambitDebugGoldLineType::BaseReward,
		TEXT("economy.round_reward"),
		TEXT("Round reward"),
		BaseGoldReward,
		BaseBefore,
		CurrentGold,
		FString::Printf(TEXT("Base reward from round score: %+d gold"), BaseGoldReward));

	const int32 RecurringBefore = CurrentGold;
	const int32 RecurringGold = ApplyRecurringGoldIncome();
	if (RecurringGold != 0)
	{
		AddGoldDebugLine(
			OutGoldLines,
			EGambitDebugGoldLineType::RecurringIncome,
			TEXT("economy.recurring_income"),
			TEXT("Recurring income"),
			RecurringGold,
			RecurringBefore,
			CurrentGold,
			FString::Printf(TEXT("Recurring income total: %+d gold"), RecurringGold));
	}

	const int32 InterestBonus = CalculateInterestGoldBonus(CurrentGold);
	const int32 InterestBefore = CurrentGold;
	AddGold(InterestBonus);
	AddGoldDebugLine(
		OutGoldLines,
		EGambitDebugGoldLineType::Interest,
		TEXT("economy.interest"),
		TEXT("Interest"),
		InterestBonus,
		InterestBefore,
		CurrentGold,
		FString::Printf(TEXT("Interest from saved gold: %+d gold"), InterestBonus));
	return InterestBonus;
}

void UGambitEconomyComponent::ModifyDebtLimit(const int32 DebtLimitDelta)
{
	DebtLimit = FMath::Max(0, DebtLimit + DebtLimitDelta);
	CurrentGold = ClampGold(CurrentGold);
	OnGoldChanged.Broadcast(CurrentGold);
}

void UGambitEconomyComponent::SetDebtLimitModifier(const FName SourceId, const int32 DebtLimitDelta)
{
	if (SourceId.IsNone())
	{
		ModifyDebtLimit(DebtLimitDelta);
		return;
	}

	DebtLimitModifiers.Add(SourceId, FMath::Max(0, DebtLimitDelta));
	CurrentGold = ClampGold(CurrentGold);
	OnGoldChanged.Broadcast(CurrentGold);
}

void UGambitEconomyComponent::ModifyMaxGold(const int32 InMaxGoldDelta)
{
	MaxGoldDelta += InMaxGoldDelta;
	CurrentGold = ClampGold(CurrentGold);
	OnGoldChanged.Broadcast(CurrentGold);
}

void UGambitEconomyComponent::SetMaxGoldModifier(const FName SourceId, const int32 InMaxGoldDelta)
{
	if (SourceId.IsNone())
	{
		ModifyMaxGold(InMaxGoldDelta);
		return;
	}

	MaxGoldModifiers.Add(SourceId, InMaxGoldDelta);
	CurrentGold = ClampGold(CurrentGold);
	OnGoldChanged.Broadcast(CurrentGold);
}

void UGambitEconomyComponent::ModifyInterestInterval(const int32 InInterestIntervalDelta)
{
	InterestIntervalDelta += InInterestIntervalDelta;
}

void UGambitEconomyComponent::SetInterestIntervalModifier(const FName SourceId, const int32 InInterestIntervalDelta)
{
	if (SourceId.IsNone())
	{
		ModifyInterestInterval(InInterestIntervalDelta);
		return;
	}

	InterestIntervalModifiers.Add(SourceId, InInterestIntervalDelta);
}

void UGambitEconomyComponent::ModifyMaxInterest(const int32 InMaxInterestDelta)
{
	MaxInterestDelta += InMaxInterestDelta;
}

void UGambitEconomyComponent::SetMaxInterestModifier(const FName SourceId, const int32 InMaxInterestDelta)
{
	if (SourceId.IsNone())
	{
		ModifyMaxInterest(InMaxInterestDelta);
		return;
	}

	MaxInterestModifiers.Add(SourceId, InMaxInterestDelta);
}

void UGambitEconomyComponent::ModifyInterestBonus(const int32 InInterestBonusDelta)
{
	InterestBonusDelta += InInterestBonusDelta;
}

void UGambitEconomyComponent::SetInterestBonusModifier(const FName SourceId, const int32 InInterestBonusDelta)
{
	if (SourceId.IsNone())
	{
		ModifyInterestBonus(InInterestBonusDelta);
		return;
	}

	InterestBonusModifiers.Add(SourceId, InInterestBonusDelta);
}

void UGambitEconomyComponent::AddRecurringGoldIncome(
	const FName SourceId,
	const int32 GoldPerRound,
	const int32 RoundCount)
{
	if (GoldPerRound == 0 || RoundCount <= 0)
	{
		return;
	}

	FGambitRecurringGoldIncome Income;
	Income.SourceId = SourceId;
	Income.GoldPerRound = GoldPerRound;
	Income.RemainingRounds = RoundCount;
	RecurringGoldIncomes.Add(Income);
}

int32 UGambitEconomyComponent::GetEffectiveMaxGold() const
{
	return FMath::Max(1, UGambitGameBalanceSettings::Get()->MaxGold + MaxGoldDelta + SumModifierMap(MaxGoldModifiers));
}

int32 UGambitEconomyComponent::GetEffectiveMinimumGold() const
{
	return -FMath::Max(0, DebtLimit + SumModifierMap(DebtLimitModifiers));
}

int32 UGambitEconomyComponent::GetEffectiveInterestInterval() const
{
	return FMath::Max(1, UGambitGameBalanceSettings::Get()->InterestIntervalGold + InterestIntervalDelta + SumModifierMap(InterestIntervalModifiers));
}

int32 UGambitEconomyComponent::GetEffectiveMaxInterest() const
{
	return FMath::Max(0, UGambitGameBalanceSettings::Get()->MaxInterestGold + MaxInterestDelta + SumModifierMap(MaxInterestModifiers));
}

int32 UGambitEconomyComponent::ClampGold(const int32 InGold) const
{
	return FMath::Clamp(InGold, GetEffectiveMinimumGold(), GetEffectiveMaxGold());
}

int32 UGambitEconomyComponent::ApplyRecurringGoldIncome()
{
	int32 TotalRecurringGold = 0;
	for (int32 Index = RecurringGoldIncomes.Num() - 1; Index >= 0; --Index)
	{
		FGambitRecurringGoldIncome& Income = RecurringGoldIncomes[Index];
		if (Income.RemainingRounds <= 0)
		{
			RecurringGoldIncomes.RemoveAtSwap(Index);
			continue;
		}

		TotalRecurringGold += Income.GoldPerRound;
		Income.RemainingRounds--;
		if (Income.RemainingRounds <= 0)
		{
			RecurringGoldIncomes.RemoveAtSwap(Index);
		}
	}

	if (TotalRecurringGold != 0)
	{
		AddGold(TotalRecurringGold);
	}

	return TotalRecurringGold;
}

int32 UGambitEconomyComponent::CalculateInterestGoldBonus(const int32 GoldAfterReward) const
{
	const int32 Interest = GoldAfterReward / GetEffectiveInterestInterval();
	return FMath::Max(0, FMath::Clamp(Interest, 0, GetEffectiveMaxInterest()) + InterestBonusDelta + SumModifierMap(InterestBonusModifiers));
}

int32 UGambitEconomyComponent::SumModifierMap(const TMap<FName, int32>& ModifierMap)
{
	int32 Total = 0;
	for (const TPair<FName, int32>& Entry : ModifierMap)
	{
		Total += Entry.Value;
	}
	return Total;
}
