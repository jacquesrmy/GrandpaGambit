#include "Items/Data/GambitItemDefinition.h"

#if WITH_EDITOR
#include "Data/Validation/GambitDataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult UGambitItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	TArray<FGambitDataValidationIssue> Issues;
	GambitDataValidation::ValidateItemDefinition(this, Issues);
	return GambitDataValidation::ApplyIssuesToContext(Issues, Context, SuperResult);
}
#endif

FName UGambitItemDefinition::GetStableItemId() const
{
	return ItemId;
}

FName UGambitItemDefinition::GetResolvedItemId() const
{
	if (!ItemId.IsNone())
	{
		return ItemId;
	}

	const FPrimaryAssetId PrimaryAssetId = GetPrimaryAssetId();
	if (PrimaryAssetId.IsValid())
	{
		return PrimaryAssetId.PrimaryAssetName;
	}

	return FName(*GetName());
}
