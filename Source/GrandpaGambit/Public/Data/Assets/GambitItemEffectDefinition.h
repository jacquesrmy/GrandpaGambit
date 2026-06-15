#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Items/Effects/GambitEffectExecutionTypes.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "GambitItemEffectDefinition.generated.h"

class UGambitConsumableDefinition;
class UGambitDiceDefinition;
class UGambitItemDefinition;

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitItemEffectDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION()
	static TArray<FName> GetTargetRuleIdOptions();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	EGambitEffectHook Hook = EGambitEffectHook::ScoreModifier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	EGambitItemEffectType EffectType = EGambitItemEffectType::ScoreModifier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	EGambitEffectTarget Target = EGambitEffectTarget::Source;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	FName EffectId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	FName EffectTypeId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (GetOptions = "GetTargetRuleIdOptions"))
	FName TargetRuleId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	FGambitScoreModifierContext ScoreModifier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TArray<FGambitEffectConditionDefinition> Conditions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	float Amount = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	float Multiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	int32 DieIndex = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	int32 DieValue = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	int32 ScoreContributionValue = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0"))
	int32 ComboContributionCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	bool bRuntimeBoolValue = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	bool bAffectAllDice = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	EGambitDiceType TransformDiceType = EGambitDiceType::D6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	int32 TransformMinValue = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	int32 TransformMaxValue = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TObjectPtr<UGambitDiceDefinition> TransformDiceDefinition = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TObjectPtr<UGambitConsumableDefinition> GrantedConsumableDefinition = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TObjectPtr<UGambitItemDefinition> ReferencedItemDefinition = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0"))
	int32 DurationRounds = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TArray<FName> RuntimeTagsToAdd;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TArray<FName> RuntimeTagsToRemove;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	bool bNegativeEffect = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TMap<FName, float> ScalarParameters;
};
