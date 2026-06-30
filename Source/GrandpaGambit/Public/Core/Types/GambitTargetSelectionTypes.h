#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Items/Effects/GambitEffectExecutionTypes.h"
#include "GambitTargetSelectionTypes.generated.h"

class AGambitPlayerState;
class UGambitItemDefinition;
class UGambitItemEffectDefinition;

UENUM(BlueprintType)
enum class EGambitTargetSelectionType : uint8
{
	None,
	Player,
	Die,
	PlayerAndDie,
	MultiTargetPreview
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitTargetSelectionOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	int32 OptionId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	EGambitTargetSelectionType SelectionType = EGambitTargetSelectionType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	TObjectPtr<AGambitPlayerState> TargetPlayer = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	int32 TargetPlayerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	int32 TargetDieHandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	int32 TargetDieInstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	TArray<int32> PreviewDieHandIndexes;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FName TargetRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FString PresentationText;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitTargetSelectionRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	TObjectPtr<AGambitPlayerState> RequestingPlayer = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	int32 RequestingPlayerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	int32 SourceConsumableSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FGuid SourceInventoryInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	TObjectPtr<UGambitItemDefinition> SourceItemDefinition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	TObjectPtr<UGambitItemEffectDefinition> SourceEffectDefinition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FName SourceItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FName EffectId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FName TargetRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	EGambitTargetSelectionType SelectionType = EGambitTargetSelectionType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	EGambitEffectTarget RequestedEffectTarget = EGambitEffectTarget::Source;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	EGambitRoundPhase CurrentPhase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	bool bRequiresExplicitSelection = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	bool bHasValidOptions = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	TArray<FGambitTargetSelectionOption> Options;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FString InvalidReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FString PresentationText;

	bool HasValidOptions() const
	{
		return bHasValidOptions && Options.ContainsByPredicate([](const FGambitTargetSelectionOption& Option)
		{
			return Option.bValid;
		});
	}
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitTargetSelectionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	bool bConfirmed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	int32 SourceConsumableSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	int32 SelectedOptionId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	TObjectPtr<AGambitPlayerState> TargetPlayer = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	int32 TargetPlayerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	int32 TargetDieHandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	int32 TargetDieInstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FName TargetRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Target Selection")
	FString FailureReason;
};
