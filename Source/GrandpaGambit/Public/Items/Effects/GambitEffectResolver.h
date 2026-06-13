#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Items/Effects/GambitEffectExecutionTypes.h"
#include "GambitEffectResolver.generated.h"

class UGambitItemDefinition;
class UGambitItemEffectDefinition;
class UGambitDiceDefinition;

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitEffectResolver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gambit|Effects")
	int32 ExecuteItemEffects(UGambitItemDefinition* ItemDefinition, UPARAM(ref) FGambitEffectExecutionContext& Context) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Effects")
	int32 ExecuteDiceEffects(UGambitDiceDefinition* DiceDefinition, UPARAM(ref) FGambitEffectExecutionContext& Context) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Effects")
	bool ExecuteEffectDefinition(UGambitItemEffectDefinition* EffectDefinition, UPARAM(ref) FGambitEffectExecutionContext& Context) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Effects")
	static FGambitScoreModifierContext MergeScoreModifiers(
		const FGambitScoreModifierContext& A,
		const FGambitScoreModifierContext& B);

private:
	bool AreConditionsMet(const UGambitItemEffectDefinition* EffectDefinition, FGambitEffectExecutionContext& Context) const;
	bool IsConditionMet(const FGambitEffectConditionDefinition& Condition, FGambitEffectExecutionContext& Context) const;
	bool ApplyEffectDefinition(UGambitItemEffectDefinition* EffectDefinition, FGambitEffectExecutionContext& Context) const;
	bool ApplyEffectToTarget(UGambitItemEffectDefinition* EffectDefinition, EGambitEffectTarget Target, FGambitEffectExecutionContext& Context) const;

	static bool CompareInt(int32 Left, EGambitEffectComparison Comparison, int32 Right);
	static bool CompareRarity(EGambitItemRarity Left, EGambitEffectComparison Comparison, EGambitItemRarity Right);
	static FString EffectHookToString(EGambitEffectHook Hook);
	static FString EffectTypeToString(EGambitItemEffectType EffectType);
};
