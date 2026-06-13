#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitGameplayTypes.h"

class GRANDPAGAMBIT_API FGambitRoundPhaseRules
{
public:
	static bool IsTransitionAllowed(EGambitRoundPhase CurrentPhase, EGambitRoundPhase NextPhase);
};
