#include "Core/Settings/GambitLocalMultiplayerSettings.h"

UGambitLocalMultiplayerSettings::UGambitLocalMultiplayerSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("GrandpaGambit.LocalMultiplayer");
}

const UGambitLocalMultiplayerSettings* UGambitLocalMultiplayerSettings::Get()
{
	return GetDefault<UGambitLocalMultiplayerSettings>();
}

bool UGambitLocalMultiplayerSettings::ShouldUseSplitScreen(const int32 LocalPlayerCount) const
{
	switch (ViewportLayoutMode)
	{
	case EGambitViewportLayoutMode::ForceSplitScreen:
		return LocalPlayerCount > 1;
	case EGambitViewportLayoutMode::ForceSharedCamera:
		return false;
	case EGambitViewportLayoutMode::Auto:
	default:
		return bEnableSplitScreenInAuto && LocalPlayerCount > 1;
	}
}

bool UGambitLocalMultiplayerSettings::IsSupportedLocalPlayerCount(const int32 LocalPlayerCount) const
{
	const int32 ClampedMinPlayers = FMath::Clamp(MinLocalPlayers, 1, 4);
	const int32 ClampedMaxPlayers = FMath::Clamp(MaxLocalPlayers, ClampedMinPlayers, 4);
	return LocalPlayerCount >= ClampedMinPlayers && LocalPlayerCount <= ClampedMaxPlayers;
}

int32 UGambitLocalMultiplayerSettings::ClampLocalPlayerCount(const int32 RequestedLocalPlayerCount) const
{
	const int32 ClampedMinPlayers = FMath::Clamp(MinLocalPlayers, 1, 4);
	const int32 ClampedMaxPlayers = FMath::Clamp(MaxLocalPlayers, ClampedMinPlayers, 4);
	return FMath::Clamp(RequestedLocalPlayerCount, ClampedMinPlayers, ClampedMaxPlayers);
}
