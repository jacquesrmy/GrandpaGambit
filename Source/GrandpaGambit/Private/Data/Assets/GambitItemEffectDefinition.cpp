#include "Data/Assets/GambitItemEffectDefinition.h"

#include "Items/Effects/GambitEffectTargetRules.h"

#if WITH_EDITOR
#include "Data/Validation/GambitDataValidation.h"
#endif

TArray<FName> UGambitItemEffectDefinition::GetTargetRuleIdOptions()
{
	return GambitEffectTargetRules::GetAuthorableRuleIds();
}

#if WITH_EDITOR
EDataValidationResult UGambitItemEffectDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	TArray<FGambitDataValidationIssue> Issues;
	GambitDataValidation::ValidateEffectDefinition(this, TEXT("EffectAsset"), 0, Issues);
	return GambitDataValidation::ApplyIssuesToContext(Issues, Context, SuperResult);
}
#endif
