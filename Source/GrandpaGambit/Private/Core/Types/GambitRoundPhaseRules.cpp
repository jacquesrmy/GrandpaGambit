#include "Core/Types/GambitRoundPhaseRules.h"

bool FGambitRoundPhaseRules::IsTransitionAllowed(const EGambitRoundPhase CurrentPhase, const EGambitRoundPhase NextPhase)
{
	if (CurrentPhase == NextPhase)
	{
		return false;
	}

	if (CurrentPhase == EGambitRoundPhase::None)
	{
		return NextPhase == EGambitRoundPhase::Roll;
	}

	if (CurrentPhase == EGambitRoundPhase::RoundEnd)
	{
		return NextPhase == EGambitRoundPhase::Roll || NextPhase == EGambitRoundPhase::None;
	}

	if (CurrentPhase == EGambitRoundPhase::Roll)
	{
		return NextPhase == EGambitRoundPhase::SelectionReroll;
	}

	if (CurrentPhase == EGambitRoundPhase::SelectionReroll)
	{
		return NextPhase == EGambitRoundPhase::Action;
	}

	if (CurrentPhase == EGambitRoundPhase::Action)
	{
		return NextPhase == EGambitRoundPhase::Resolution;
	}

	if (CurrentPhase == EGambitRoundPhase::Resolution)
	{
		return NextPhase == EGambitRoundPhase::Reward;
	}

	if (CurrentPhase == EGambitRoundPhase::Reward)
	{
		return NextPhase == EGambitRoundPhase::Ranking;
	}

	if (CurrentPhase == EGambitRoundPhase::Ranking)
	{
		return NextPhase == EGambitRoundPhase::Shop;
	}

	if (CurrentPhase == EGambitRoundPhase::Shop)
	{
		return NextPhase == EGambitRoundPhase::RoundEnd;
	}

	return false;
}
