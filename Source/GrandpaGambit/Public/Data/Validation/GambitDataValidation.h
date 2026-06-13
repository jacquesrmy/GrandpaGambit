#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Items/Effects/GambitEffectExecutionTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

class UGambitDiceDefinition;
class UGambitItemCatalogDataAsset;
class UGambitItemDefinition;
class UGambitItemEffectDefinition;
class UGambitSharedPoolDefinition;
class UGambitShopLootTable;

enum class EGambitDataValidationSeverity : uint8
{
	Info,
	Warning,
	Error
};

struct GRANDPAGAMBIT_API FGambitDataValidationIssue
{
	FGambitDataValidationIssue() = default;

	FGambitDataValidationIssue(const EGambitDataValidationSeverity InSeverity, FString InMessage)
		: Severity(InSeverity)
		, Message(MoveTemp(InMessage))
	{
	}

	bool IsBlocking() const
	{
		return Severity == EGambitDataValidationSeverity::Error;
	}

	EGambitDataValidationSeverity Severity = EGambitDataValidationSeverity::Info;
	FString Message;
};

namespace GambitDataValidation
{
	GRANDPAGAMBIT_API void AddIssue(
		TArray<FGambitDataValidationIssue>& OutIssues,
		EGambitDataValidationSeverity Severity,
		const FString& Message);

	GRANDPAGAMBIT_API bool HasBlockingIssues(const TArray<FGambitDataValidationIssue>& Issues);
	GRANDPAGAMBIT_API FString SeverityToString(EGambitDataValidationSeverity Severity);
	GRANDPAGAMBIT_API FString EffectTypeToString(EGambitItemEffectType EffectType);
	GRANDPAGAMBIT_API FString EffectHookToString(EGambitEffectHook Hook);
	GRANDPAGAMBIT_API FString EffectTargetToString(EGambitEffectTarget Target);
	GRANDPAGAMBIT_API bool IsSupportedEffectType(EGambitItemEffectType EffectType);

	GRANDPAGAMBIT_API void ValidateDiceDefinition(
		const UGambitDiceDefinition* DiceDefinition,
		TArray<FGambitDataValidationIssue>& OutIssues);

	GRANDPAGAMBIT_API void ValidateItemDefinition(
		const UGambitItemDefinition* ItemDefinition,
		TArray<FGambitDataValidationIssue>& OutIssues);

	GRANDPAGAMBIT_API void ValidateEffectDefinition(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FString& OwnerLabel,
		int32 EffectIndex,
		TArray<FGambitDataValidationIssue>& OutIssues);

	GRANDPAGAMBIT_API void ValidateShopLootTable(
		const UGambitShopLootTable* LootTable,
		const UGambitItemCatalogDataAsset* FallbackCatalog,
		TArray<FGambitDataValidationIssue>& OutIssues);

	GRANDPAGAMBIT_API void ValidateSharedPoolDefinition(
		const UGambitSharedPoolDefinition* SharedPoolDefinition,
		const UGambitItemCatalogDataAsset* FallbackCatalog,
		TArray<FGambitDataValidationIssue>& OutIssues);

#if WITH_EDITOR
	GRANDPAGAMBIT_API EDataValidationResult ApplyIssuesToContext(
		const TArray<FGambitDataValidationIssue>& Issues,
		FDataValidationContext& Context,
		EDataValidationResult DefaultResult);
#endif
}
