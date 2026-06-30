#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/GambitRoundFeedbackTypes.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitEconomyComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGambitGoldChanged, int32, NewGold);

UCLASS(ClassGroup = (Gambit), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GRANDPAGAMBIT_API UGambitEconomyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGambitEconomyComponent();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Economy")
	void InitializeForMatch();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Economy")
	int32 AddGold(int32 DeltaGold);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Economy")
	bool SpendGold(int32 Cost);

	UFUNCTION(BlueprintPure, Category = "Gambit|Economy")
	bool CanSpendGold(int32 Cost) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Economy")
	int32 ApplyRoundEconomy(int32 BaseGoldReward);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Economy")
	int32 ApplyRoundEconomyDetailed(int32 BaseGoldReward, UPARAM(ref) TArray<FGambitGoldBreakdownLine>& OutGoldLines);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Economy")
	void ModifyDebtLimit(int32 DebtLimitDelta);

	void SetDebtLimitModifier(FName SourceId, int32 DebtLimitDelta);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Economy")
	void ModifyMaxGold(int32 MaxGoldDelta);

	void SetMaxGoldModifier(FName SourceId, int32 InMaxGoldDelta);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Economy")
	void ModifyInterestInterval(int32 InterestIntervalDelta);

	void SetInterestIntervalModifier(FName SourceId, int32 InInterestIntervalDelta);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Economy")
	void ModifyMaxInterest(int32 MaxInterestDelta);

	void SetMaxInterestModifier(FName SourceId, int32 InMaxInterestDelta);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Economy")
	void ModifyInterestBonus(int32 InterestBonusDelta);

	void SetInterestBonusModifier(FName SourceId, int32 InInterestBonusDelta);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Economy")
	void AddRecurringGoldIncome(FName SourceId, int32 GoldPerRound, int32 RoundCount);

	UFUNCTION(BlueprintPure, Category = "Gambit|Economy")
	int32 GetCurrentGold() const { return CurrentGold; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Economy")
	int32 GetEffectiveMaxGold() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Economy")
	int32 GetEffectiveMinimumGold() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Economy")
	int32 GetEffectiveInterestInterval() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Economy")
	int32 GetEffectiveMaxInterest() const;

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Economy")
	FOnGambitGoldChanged OnGoldChanged;

private:
	int32 ClampGold(int32 InGold) const;
	int32 ApplyRecurringGoldIncome();
	int32 CalculateInterestGoldBonus(int32 GoldAfterReward) const;
	static int32 SumModifierMap(const TMap<FName, int32>& ModifierMap);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Economy", meta = (AllowPrivateAccess = "true"))
	int32 CurrentGold = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Economy", meta = (AllowPrivateAccess = "true"))
	int32 DebtLimit = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Economy", meta = (AllowPrivateAccess = "true"))
	int32 MaxGoldDelta = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Economy", meta = (AllowPrivateAccess = "true"))
	int32 InterestIntervalDelta = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Economy", meta = (AllowPrivateAccess = "true"))
	int32 MaxInterestDelta = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Economy", meta = (AllowPrivateAccess = "true"))
	int32 InterestBonusDelta = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Economy", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitRecurringGoldIncome> RecurringGoldIncomes;

	TMap<FName, int32> DebtLimitModifiers;
	TMap<FName, int32> MaxGoldModifiers;
	TMap<FName, int32> InterestIntervalModifiers;
	TMap<FName, int32> MaxInterestModifiers;
	TMap<FName, int32> InterestBonusModifiers;
};
