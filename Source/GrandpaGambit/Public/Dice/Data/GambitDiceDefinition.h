#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/Types/GambitGameplayTypes.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "GambitDiceDefinition.generated.h"

class UGambitItemEffect;
class UGambitItemEffectDefinition;

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitDiceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION(BlueprintPure, Category = "Dice")
	FName GetStableDiceId() const;

	UFUNCTION(BlueprintPure, Category = "Dice")
	FName GetResolvedDiceId() const;

	UFUNCTION(BlueprintPure, Category = "Dice")
	TArray<int32> GetResolvedFaces() const;

	UFUNCTION(BlueprintPure, Category = "Dice")
	int32 GetFaceValue(int32 FaceIndex) const;

	UFUNCTION(BlueprintPure, Category = "Dice")
	float GetFaceWeight(int32 FaceIndex) const;

	int32 RollFaceIndex(FRandomStream& RandomStream) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dice")
	FName DiceId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dice")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dice", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dice")
	EGambitDiceType DiceType = EGambitDiceType::D6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dice")
	FName DiceTypeId = TEXT("dice.standard.d6");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dice")
	EGambitItemRarity Rarity = EGambitItemRarity::Common;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dice")
	FName RarityId = TEXT("rarity.common");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dice")
	TArray<FName> Tags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Faces")
	TArray<int32> Faces;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Faces")
	bool bAllowDefaultFacesFallback = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Faces", meta = (ClampMin = "0.0"))
	TArray<float> FaceWeights;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runtime Rules", meta = (ClampMin = "0"))
	int32 DefaultComboContributionCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runtime Rules")
	bool bCountsForScoreSum = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runtime Rules")
	bool bCountsForCombinations = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runtime Rules")
	bool bCanBeRerolled = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runtime Rules")
	bool bCanBeLocked = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runtime Rules")
	bool bDestroyedAfterRound = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runtime Rules")
	bool bOverrideScoreContributionValue = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runtime Rules", meta = (EditCondition = "bOverrideScoreContributionValue"))
	int32 ScoreContributionValueOverride = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TArray<TSubclassOf<UGambitItemEffect>> EffectClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TArray<TObjectPtr<UGambitItemEffectDefinition>> EffectDefinitions;
};
