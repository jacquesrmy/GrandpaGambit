#include "Data/Assets/GambitSharedPoolDefinition.h"

#if WITH_EDITOR
#include "Data/Validation/GambitDataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult UGambitSharedPoolDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	TArray<FGambitDataValidationIssue> Issues;
	GambitDataValidation::ValidateSharedPoolDefinition(this, ItemCatalog.Get(), Issues);
	return GambitDataValidation::ApplyIssuesToContext(Issues, Context, SuperResult);
}
#endif
