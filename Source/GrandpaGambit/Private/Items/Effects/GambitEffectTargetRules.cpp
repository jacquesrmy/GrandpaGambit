#include "Items/Effects/GambitEffectTargetRules.h"

namespace GambitEffectTargetRules
{
	const FName SelectedDie(TEXT("selected_die"));
	const FName SourceSelectedDie(TEXT("source.selected_die"));
	const FName TargetSelectedDie(TEXT("target.selected_die"));
	const FName FirstRerolledDie(TEXT("first_rerolled_die"));
	const FName FirstRerolledDieThisRound(TEXT("first_rerolled_die_this_round"));
	const FName TargetOpponent(TEXT("target.opponent"));
	const FName AllOpponents(TEXT("all_opponents"));
	const FName LeadingPlayer(TEXT("leading_player"));
	const FName RichestPlayer(TEXT("richest_player"));
	const FName PoorestPlayer(TEXT("poorest_player"));
	const FName LowestScorePlayer(TEXT("lowest_score_player"));
	const FName LeftOpponent(TEXT("left_opponent"));
	const FName RightOpponent(TEXT("right_opponent"));
	const FName OppositePlayer(TEXT("opposite_player"));
	const FName RandomOpponent(TEXT("random_opponent"));
	const FName SourceRandomDie(TEXT("source.random_die"));
	const FName SourceBestDie(TEXT("source.best_die"));
	const FName SourceLowestDie(TEXT("source.lowest_die"));
	const FName SourceAllDice(TEXT("source.all_dice"));
	const FName TargetRandomDie(TEXT("target.random_die"));
	const FName TargetBestDie(TEXT("target.best_die"));
	const FName TargetLowestDie(TEXT("target.lowest_die"));
	const FName TargetAllDice(TEXT("target.all_dice"));

	namespace
	{
		const TArray<FRuleMetadata>& GetRuleMetadataTable()
		{
			static const TArray<FRuleMetadata> RuleMetadata = {
				{SelectedDie, TEXT("Selected die on the requested source/target side."), ERuleCategory::DieTarget, false, false, true},
				{SourceSelectedDie, TEXT("Selected die on the source player."), ERuleCategory::DieTarget, false, false, true},
				{TargetSelectedDie, TEXT("Selected die on the explicit target player."), ERuleCategory::DieTarget, false, true, true},
				{FirstRerolledDie, TEXT("First rerolled die on the requested source/target side."), ERuleCategory::DieTarget, false, false, true},
				{FirstRerolledDieThisRound, TEXT("First die rerolled by the source during this round."), ERuleCategory::DieTarget, false, false, true},
				{TargetOpponent, TEXT("Explicit target opponent, never the source player."), ERuleCategory::PlayerTarget, false, true, false},
				{AllOpponents, TEXT("Every match player except the source player."), ERuleCategory::MultiTarget, true, false, false},
				{LeadingPlayer, TEXT("Player with the highest victory points, then highest current round score, then stable table order."), ERuleCategory::PlayerTarget, false, false, false},
				{RichestPlayer, TEXT("Player with the most gold; ties use stable table order."), ERuleCategory::PlayerTarget, false, false, false},
				{PoorestPlayer, TEXT("Player with the least gold; ties use stable table order."), ERuleCategory::PlayerTarget, false, false, false},
				{LowestScorePlayer, TEXT("Player with the lowest current round score; ties use stable table order."), ERuleCategory::PlayerTarget, false, false, false},
				{LeftOpponent, TEXT("Previous opponent in stable match player order."), ERuleCategory::PlayerTarget, false, false, false},
				{RightOpponent, TEXT("Next opponent in stable match player order."), ERuleCategory::PlayerTarget, false, false, false},
				{OppositePlayer, TEXT("Player opposite the source in a four-player stable match order."), ERuleCategory::PlayerTarget, false, false, false},
				{RandomOpponent, TEXT("Random opponent chosen from stable match player order."), ERuleCategory::PlayerTarget, false, false, false},
				{SourceRandomDie, TEXT("Random die on the source player."), ERuleCategory::DieTarget, false, false, true},
				{SourceBestDie, TEXT("Highest-value die on the source player; ties use lower hand index."), ERuleCategory::DieTarget, false, false, true},
				{SourceLowestDie, TEXT("Lowest-value die on the source player; ties use lower hand index."), ERuleCategory::DieTarget, false, false, true},
				{SourceAllDice, TEXT("All dice on the source player."), ERuleCategory::MultiTarget, true, false, true},
				{TargetRandomDie, TEXT("Random die on the explicit target player."), ERuleCategory::DieTarget, false, true, true},
				{TargetBestDie, TEXT("Highest-value die on the explicit target player; ties use lower hand index."), ERuleCategory::DieTarget, false, true, true},
				{TargetLowestDie, TEXT("Lowest-value die on the explicit target player; ties use lower hand index."), ERuleCategory::DieTarget, false, true, true},
				{TargetAllDice, TEXT("All dice on the explicit target player."), ERuleCategory::MultiTarget, true, true, true},
			};
			return RuleMetadata;
		}
	}

	TArray<FName> GetKnownRuleIds()
	{
		TArray<FName> RuleIds;
		const TArray<FRuleMetadata>& RuleMetadata = GetRuleMetadataTable();
		RuleIds.Reserve(RuleMetadata.Num());
		for (const FRuleMetadata& Metadata : RuleMetadata)
		{
			RuleIds.Add(Metadata.Id);
		}
		return RuleIds;
	}

	TArray<FName> GetAuthorableRuleIds()
	{
		TArray<FName> RuleIds;
		RuleIds.Reserve(GetRuleMetadataTable().Num() + 1);
		RuleIds.Add(NAME_None);
		RuleIds.Append(GetKnownRuleIds());
		return RuleIds;
	}

	TArray<FRuleMetadata> GetKnownRuleMetadata()
	{
		return GetRuleMetadataTable();
	}

	bool GetRuleMetadata(const FName TargetRuleId, FRuleMetadata& OutMetadata)
	{
		for (const FRuleMetadata& Metadata : GetRuleMetadataTable())
		{
			if (Metadata.Id == TargetRuleId)
			{
				OutMetadata = Metadata;
				return true;
			}
		}

		return false;
	}

	bool IsKnownRule(const FName TargetRuleId)
	{
		FRuleMetadata Metadata;
		return GetRuleMetadata(TargetRuleId, Metadata);
	}

	bool IsSelectedDieRule(const FName TargetRuleId)
	{
		return TargetRuleId == SelectedDie
			|| TargetRuleId == SourceSelectedDie
			|| TargetRuleId == TargetSelectedDie;
	}

	bool IsFirstRerolledDieRule(const FName TargetRuleId)
	{
		return TargetRuleId == FirstRerolledDie
			|| TargetRuleId == FirstRerolledDieThisRound;
	}

	bool IsOpponentRule(const FName TargetRuleId)
	{
		return TargetRuleId == TargetOpponent
			|| TargetRuleId == AllOpponents
			|| TargetRuleId == LeftOpponent
			|| TargetRuleId == RightOpponent
			|| TargetRuleId == OppositePlayer
			|| TargetRuleId == RandomOpponent;
	}

	bool IsPlayerRule(const FName TargetRuleId)
	{
		FRuleMetadata Metadata;
		if (!GetRuleMetadata(TargetRuleId, Metadata))
		{
			return false;
		}

		return Metadata.Category == ERuleCategory::PlayerTarget
			|| (Metadata.Category == ERuleCategory::MultiTarget && !Metadata.bRequiresDice);
	}

	bool IsDieRule(const FName TargetRuleId)
	{
		FRuleMetadata Metadata;
		if (!GetRuleMetadata(TargetRuleId, Metadata))
		{
			return false;
		}

		return Metadata.Category == ERuleCategory::DieTarget || Metadata.bRequiresDice;
	}

	bool IsMultiTargetRule(const FName TargetRuleId)
	{
		FRuleMetadata Metadata;
		return GetRuleMetadata(TargetRuleId, Metadata) && Metadata.bCanReturnMultipleTargets;
	}

	bool RequiresExplicitTargetPlayer(const FName TargetRuleId)
	{
		FRuleMetadata Metadata;
		return GetRuleMetadata(TargetRuleId, Metadata) && Metadata.bRequiresExplicitTargetPlayer;
	}

	bool RequiresDice(const FName TargetRuleId)
	{
		FRuleMetadata Metadata;
		return GetRuleMetadata(TargetRuleId, Metadata) && Metadata.bRequiresDice;
	}

	bool RequiresSelectedDie(const FName TargetRuleId)
	{
		return IsSelectedDieRule(TargetRuleId);
	}

	FString DescribeRule(const FName TargetRuleId)
	{
		if (TargetRuleId.IsNone())
		{
			return TEXT("No target rule");
		}

		FRuleMetadata Metadata;
		if (GetRuleMetadata(TargetRuleId, Metadata))
		{
			return Metadata.Description;
		}

		return FString::Printf(TEXT("Unknown target rule '%s'"), *TargetRuleId.ToString());
	}
}
