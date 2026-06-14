#include "Players/Components/GambitDiceComponent.h"

#include "Core/Settings/GambitGameBalanceSettings.h"
#include "Dice/Data/GambitDiceDefinition.h"

namespace
{
	const TArray<int32>& GetDiceComponentFallbackD6Faces()
	{
		static const TArray<int32> FallbackFaces = { 1, 2, 3, 4, 5, 6 };
		return FallbackFaces;
	}

	int32 RollFallbackFaceIndex(FRandomStream& RandomStream)
	{
		return RandomStream.RandRange(0, GetDiceComponentFallbackD6Faces().Num() - 1);
	}

	int32 GetFallbackFaceValue(const int32 FaceIndex)
	{
		const TArray<int32>& Faces = GetDiceComponentFallbackD6Faces();
		return Faces.IsValidIndex(FaceIndex) ? Faces[FaceIndex] : Faces[0];
	}

	void ApplyRuntimeRulesFromDefinition(FGambitDieRuntimeState& State, const UGambitDiceDefinition* Definition)
	{
		if (!Definition)
		{
			State.ComboContributionCount = 1;
			State.bCountsForScoreSum = true;
			State.bCountsForCombinations = true;
			State.bCanBeRerolled = true;
			State.bCanBeLocked = true;
			State.bDestroyedAfterRound = false;
			State.RuntimeTags.Reset();
			return;
		}

		State.ComboContributionCount = FMath::Max(0, Definition->DefaultComboContributionCount);
		State.bCountsForScoreSum = Definition->bCountsForScoreSum;
		State.bCountsForCombinations = Definition->bCountsForCombinations;
		State.bCanBeRerolled = Definition->bCanBeRerolled;
		State.bCanBeLocked = Definition->bCanBeLocked;
		State.bDestroyedAfterRound = Definition->bDestroyedAfterRound;
		State.RuntimeTags = Definition->Tags;
		if (!State.bCanBeLocked)
		{
			State.bLocked = false;
		}
	}

	void RefreshScoreContribution(FGambitDieRuntimeState& State)
	{
		if (const UGambitDiceDefinition* Definition = State.DiceDefinition.Get())
		{
			State.ScoreContributionValue = Definition->bOverrideScoreContributionValue
				? Definition->ScoreContributionValueOverride
				: State.EffectiveValue;
			return;
		}

		State.ScoreContributionValue = State.EffectiveValue;
	}
}

UGambitDiceComponent::UGambitDiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGambitDiceComponent::InitializeDicePool(const TArray<TObjectPtr<UGambitDiceDefinition>>& OwnedDiceDefinitions)
{
	const TArray<FGambitDieRuntimeState> PreviousDiceStates = DiceStates;
	DiceStates.Reset();

	if (OwnedDiceDefinitions.Num() > 0)
	{
		for (const TObjectPtr<UGambitDiceDefinition>& OwnedDiceDefinition : OwnedDiceDefinitions)
		{
			const int32 HandIndex = DiceStates.Num();
			const FGambitDieRuntimeState* PreviousState = PreviousDiceStates.IsValidIndex(HandIndex) ? &PreviousDiceStates[HandIndex] : nullptr;
			DiceStates.Add(MakeRuntimeDie(OwnedDiceDefinition.Get(), HandIndex, PreviousState));
		}
	}

	const int32 DefaultDiceCount = FMath::Max(0, UGambitGameBalanceSettings::Get()->DefaultActiveDiceCount);
	while (DiceStates.Num() < DefaultDiceCount)
	{
		const int32 HandIndex = DiceStates.Num();
		const FGambitDieRuntimeState* PreviousState = PreviousDiceStates.IsValidIndex(HandIndex) ? &PreviousDiceStates[HandIndex] : nullptr;
		DiceStates.Add(MakeFallbackDie(HandIndex, PreviousState));
	}

	OnDiceStateChanged.Broadcast();
}

void UGambitDiceComponent::RollAll(FRandomStream& RandomStream)
{
	for (FGambitDieRuntimeState& DieState : DiceStates)
	{
		if (DieState.bCanBeLocked)
		{
			DieState.bLocked = false;
		}

		RollDie(DieState, RandomStream);
	}

	OnDiceStateChanged.Broadcast();
}

bool UGambitDiceComponent::RollUnlocked(FRandomStream& RandomStream)
{
	bool bRolledAny = false;
	for (FGambitDieRuntimeState& DieState : DiceStates)
	{
		if (!DieState.bLocked && DieState.bCanBeRerolled)
		{
			RollDie(DieState, RandomStream);
			bRolledAny = true;
		}
	}

	if (bRolledAny)
	{
		OnDiceStateChanged.Broadcast();
	}

	return bRolledAny;
}

bool UGambitDiceComponent::RollDieAtIndex(const int32 DieIndex, FRandomStream& RandomStream)
{
	if (!DiceStates.IsValidIndex(DieIndex) || !DiceStates[DieIndex].bCanBeRerolled)
	{
		return false;
	}

	RollDie(DiceStates[DieIndex], RandomStream);
	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::SetDieLocked(const int32 DieIndex, const bool bLocked)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
	if (bLocked && !DieState.bCanBeLocked)
	{
		return false;
	}

	DieState.bLocked = bLocked;
	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::SetDieValue(const int32 DieIndex, const int32 NewValue)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
	DieState.EffectiveValue = NewValue;
	RefreshScoreContribution(DieState);
	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::ModifyDieValue(const int32 DieIndex, const int32 DeltaValue)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	return SetDieValue(DieIndex, DiceStates[DieIndex].EffectiveValue + DeltaValue);
}

bool UGambitDiceComponent::SetDieScoreContributionValue(const int32 DieIndex, const int32 NewValue)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	DiceStates[DieIndex].ScoreContributionValue = NewValue;
	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::SetDieComboContributionCount(const int32 DieIndex, const int32 NewCount)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	DiceStates[DieIndex].ComboContributionCount = FMath::Max(0, NewCount);
	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::SetDieCountsForScoreSum(const int32 DieIndex, const bool bCounts)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	DiceStates[DieIndex].bCountsForScoreSum = bCounts;
	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::SetDieCountsForCombinations(const int32 DieIndex, const bool bCounts)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	DiceStates[DieIndex].bCountsForCombinations = bCounts;
	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::SetDieCanBeRerolled(const int32 DieIndex, const bool bCanBeRerolled)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	DiceStates[DieIndex].bCanBeRerolled = bCanBeRerolled;
	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::SetDieCanBeLocked(const int32 DieIndex, const bool bCanBeLocked)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
	DieState.bCanBeLocked = bCanBeLocked;
	if (!bCanBeLocked)
	{
		DieState.bLocked = false;
	}

	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::SetDieDestroyedAfterRound(const int32 DieIndex, const bool bDestroyedAfterRound)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	DiceStates[DieIndex].bDestroyedAfterRound = bDestroyedAfterRound;
	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::AddRuntimeTags(const int32 DieIndex, const TArray<FName>& TagsToAdd)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
	for (const FName Tag : TagsToAdd)
	{
		if (!Tag.IsNone())
		{
			DieState.RuntimeTags.AddUnique(Tag);
		}
	}

	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::RemoveRuntimeTags(const int32 DieIndex, const TArray<FName>& TagsToRemove)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
	for (const FName Tag : TagsToRemove)
	{
		DieState.RuntimeTags.Remove(Tag);
	}

	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::TransformDieToDefinitionForRound(
	const int32 DieIndex,
	UGambitDiceDefinition* NewDefinition,
	const bool bPreserveCurrentValue)
{
	if (!DiceStates.IsValidIndex(DieIndex) || !NewDefinition)
	{
		return false;
	}

	FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
	DieState.DiceDefinition = NewDefinition;
	ApplyRuntimeRulesFromDefinition(DieState, NewDefinition);

	if (!bPreserveCurrentValue)
	{
		DieState.RolledFaceIndex = 0;
		DieState.RawValue = NewDefinition->GetFaceValue(0);
		DieState.EffectiveValue = DieState.RawValue;
	}

	RefreshScoreContribution(DieState);
	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::AddTemporaryDie(
	UGambitDiceDefinition* DiceDefinition,
	const bool bRollImmediately,
	FRandomStream& RandomStream)
{
	const int32 HandIndex = DiceStates.Num();
	FGambitDieRuntimeState NewState = DiceDefinition
		? MakeRuntimeDie(DiceDefinition, HandIndex, nullptr)
		: MakeFallbackDie(HandIndex, nullptr);
	NewState.bDestroyedAfterRound = true;

	if (bRollImmediately)
	{
		RollDie(NewState, RandomStream);
	}

	DiceStates.Add(NewState);
	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::TransformDieForRound(
	const int32 DieIndex,
	const EGambitDiceType DiceType,
	const int32 MinValue,
	const int32 MaxValue,
	const int32 CurrentValue)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
	DieState.DiceDefinition = nullptr;
	DieState.RawValue = CurrentValue;
	DieState.EffectiveValue = CurrentValue;
	DieState.ScoreContributionValue = CurrentValue;
	DieState.RolledFaceIndex = INDEX_NONE;
	DieState.ComboContributionCount = 1;
	DieState.bCountsForScoreSum = true;
	DieState.bCountsForCombinations = true;
	DieState.bCanBeRerolled = true;
	DieState.bCanBeLocked = true;
	DieState.RuntimeTags = { FName(*StaticEnum<EGambitDiceType>()->GetNameStringByValue(static_cast<int64>(DiceType))) };
	(void)MinValue;
	(void)MaxValue;

	OnDiceStateChanged.Broadcast();
	return true;
}

bool UGambitDiceComponent::RemoveDieAtIndex(const int32 DieIndex)
{
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return false;
	}

	DiceStates.RemoveAt(DieIndex);
	RefreshHandIndexes();
	OnDiceStateChanged.Broadcast();
	return true;
}

void UGambitDiceComponent::ClearAllLocks()
{
	for (FGambitDieRuntimeState& DieState : DiceStates)
	{
		DieState.bLocked = false;
	}

	OnDiceStateChanged.Broadcast();
}

FGambitDieRuntimeState UGambitDiceComponent::MakeRuntimeDie(
	UGambitDiceDefinition* Definition,
	const int32 HandIndex,
	const FGambitDieRuntimeState* PreviousState)
{
	FGambitDieRuntimeState State;
	State.DiceDefinition = Definition;
	State.InstanceId = PreviousState ? PreviousState->InstanceId : NextDieInstanceId++;
	State.HandIndex = HandIndex;
	State.PreviousRoundValue = PreviousState ? PreviousState->EffectiveValue : INDEX_NONE;
	State.RolledFaceIndex = 0;
	State.RawValue = Definition ? Definition->GetFaceValue(0) : GetFallbackFaceValue(0);
	State.EffectiveValue = State.RawValue;
	State.bLocked = false;
	ApplyRuntimeRulesFromDefinition(State, Definition);
	RefreshScoreContribution(State);
	return State;
}

FGambitDieRuntimeState UGambitDiceComponent::MakeFallbackDie(
	const int32 HandIndex,
	const FGambitDieRuntimeState* PreviousState)
{
	FGambitDieRuntimeState State;
	State.InstanceId = PreviousState ? PreviousState->InstanceId : NextDieInstanceId++;
	State.HandIndex = HandIndex;
	State.PreviousRoundValue = PreviousState ? PreviousState->EffectiveValue : INDEX_NONE;
	State.RolledFaceIndex = 0;
	State.RawValue = GetFallbackFaceValue(0);
	State.EffectiveValue = State.RawValue;
	State.ScoreContributionValue = State.EffectiveValue;
	State.ComboContributionCount = 1;
	State.bCountsForScoreSum = true;
	State.bCountsForCombinations = true;
	State.bCanBeRerolled = true;
	State.bCanBeLocked = true;
	State.bDestroyedAfterRound = false;
	State.bLocked = false;
	return State;
}

void UGambitDiceComponent::RollDie(FGambitDieRuntimeState& DieState, FRandomStream& RandomStream)
{
	if (const UGambitDiceDefinition* Definition = DieState.DiceDefinition.Get())
	{
		DieState.RolledFaceIndex = Definition->RollFaceIndex(RandomStream);
		DieState.RawValue = Definition->GetFaceValue(DieState.RolledFaceIndex);
	}
	else
	{
		DieState.RolledFaceIndex = RollFallbackFaceIndex(RandomStream);
		DieState.RawValue = GetFallbackFaceValue(DieState.RolledFaceIndex);
	}

	DieState.EffectiveValue = DieState.RawValue;
	RefreshScoreContribution(DieState);
}

void UGambitDiceComponent::RefreshHandIndexes()
{
	for (int32 Index = 0; Index < DiceStates.Num(); ++Index)
	{
		DiceStates[Index].HandIndex = Index;
	}
}
