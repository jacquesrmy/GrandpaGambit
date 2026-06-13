#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GambitLocalPlayerInputConfig.generated.h"

class UInputMappingContext;

USTRUCT(BlueprintType)
struct FGambitInputMappingContextEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct FGambitIndexedInputMappingSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0", ClampMax = "3"))
	int32 LocalPlayerIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<FGambitInputMappingContextEntry> MappingContexts;
};

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitLocalPlayerInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<FGambitInputMappingContextEntry> SharedMappingContexts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<FGambitIndexedInputMappingSet> PlayerSpecificMappingContexts;

	const FGambitIndexedInputMappingSet* FindPlayerSpecificMappingSet(int32 LocalPlayerIndex) const;
};
