#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitGameplayEvents.generated.h"

UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGambitPhaseChanged, EGambitRoundPhase, OldPhase, EGambitRoundPhase, NewPhase);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGambitRoundChanged, int32, NewRoundIndex);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGambitRankingUpdated);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGambitRoundFlowPhaseEntered, EGambitRoundPhase, EnteredPhase);
