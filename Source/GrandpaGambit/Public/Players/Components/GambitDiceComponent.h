#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitDiceComponent.generated.h"

class UGambitDiceDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGambitDiceStateChanged);

UCLASS(ClassGroup = (Gambit), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GRANDPAGAMBIT_API UGambitDiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGambitDiceComponent();

	void InitializeDicePool(const TArray<TObjectPtr<UGambitDiceDefinition>>& OwnedDiceDefinitions);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	void RollAll(FRandomStream& RandomStream);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool RollUnlocked(FRandomStream& RandomStream);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool RollDieAtIndex(int32 DieIndex, FRandomStream& RandomStream);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool SetDieLocked(int32 DieIndex, bool bLocked);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool SetDieValue(int32 DieIndex, int32 NewValue);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool ModifyDieValue(int32 DieIndex, int32 DeltaValue);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool SetDieScoreContributionValue(int32 DieIndex, int32 NewValue);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool SetDieComboContributionCount(int32 DieIndex, int32 NewCount);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool SetDieCountsForScoreSum(int32 DieIndex, bool bCounts);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool SetDieCountsForCombinations(int32 DieIndex, bool bCounts);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool SetDieCanBeRerolled(int32 DieIndex, bool bCanBeRerolled);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool SetDieCanBeLocked(int32 DieIndex, bool bCanBeLocked);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool SetDieDestroyedAfterRound(int32 DieIndex, bool bDestroyedAfterRound);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool AddRuntimeTags(int32 DieIndex, const TArray<FName>& TagsToAdd);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool RemoveRuntimeTags(int32 DieIndex, const TArray<FName>& TagsToRemove);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool TransformDieToDefinitionForRound(int32 DieIndex, UGambitDiceDefinition* NewDefinition, bool bPreserveCurrentValue = true);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool AddTemporaryDie(UGambitDiceDefinition* DiceDefinition, bool bRollImmediately, FRandomStream& RandomStream);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool TransformDieForRound(int32 DieIndex, EGambitDiceType DiceType, int32 MinValue, int32 MaxValue, int32 CurrentValue);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	bool RemoveDieAtIndex(int32 DieIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dice")
	void ClearAllLocks();

	UFUNCTION(BlueprintPure, Category = "Gambit|Dice")
	TArray<FGambitDieRuntimeState> GetDiceStates() const { return DiceStates; }

	const TArray<FGambitDieRuntimeState>& GetDiceStatesRef() const { return DiceStates; }

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Dice")
	FOnGambitDiceStateChanged OnDiceStateChanged;

private:
	FGambitDieRuntimeState MakeRuntimeDie(UGambitDiceDefinition* Definition, int32 HandIndex, const FGambitDieRuntimeState* PreviousState);
	FGambitDieRuntimeState MakeFallbackDie(int32 HandIndex, const FGambitDieRuntimeState* PreviousState);
	void RollDie(FGambitDieRuntimeState& DieState, FRandomStream& RandomStream);
	void RefreshHandIndexes();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Dice", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitDieRuntimeState> DiceStates;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Dice", meta = (AllowPrivateAccess = "true"))
	int32 NextDieInstanceId = 0;
};
