#include "Players/Input/GambitLocalPlayerInputConfig.h"

const FGambitIndexedInputMappingSet* UGambitLocalPlayerInputConfig::FindPlayerSpecificMappingSet(const int32 LocalPlayerIndex) const
{
	return PlayerSpecificMappingContexts.FindByPredicate([LocalPlayerIndex](const FGambitIndexedInputMappingSet& Entry)
	{
		return Entry.LocalPlayerIndex == LocalPlayerIndex;
	});
}
