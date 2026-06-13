#include "Game/Camera/GambitTableCameraDirector.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AGambitTableCameraDirector::AGambitTableCameraDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGambitTableCameraDirector::ApplyCameraTargets(const TArray<ULocalPlayer*>& LocalPlayers, const bool bUseSplitScreen, const float BlendTime)
{
	for (int32 LocalPlayerIndex = 0; LocalPlayerIndex < LocalPlayers.Num(); ++LocalPlayerIndex)
	{
		ULocalPlayer* LocalPlayer = LocalPlayers[LocalPlayerIndex];
		APlayerController* PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
		if (!PlayerController)
		{
			continue;
		}

		AActor* TargetActor = nullptr;
		if (bUseSplitScreen)
		{
			if (PerPlayerCameraTargets.IsValidIndex(LocalPlayerIndex))
			{
				TargetActor = PerPlayerCameraTargets[LocalPlayerIndex];
			}
		}
		else
		{
			TargetActor = SharedCameraTarget;
		}

		if (!TargetActor && bFallbackToPawnIfMissingTarget)
		{
			TargetActor = PlayerController->GetPawn();
		}

		if (TargetActor)
		{
			PlayerController->SetViewTargetWithBlend(TargetActor, BlendTime);
		}
	}
}
