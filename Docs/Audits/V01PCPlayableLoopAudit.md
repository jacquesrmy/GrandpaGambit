# V0.1 PC Playable Loop Audit

Date: 2026-06-29

Scope: technical audit/playtest of the text-first PC playable loop for 2, 3, and 4 local players. No new content assets, no gamepad navigation, no real multi-target system, and no B7/B8/B9 work were added.

## Global status

Status: **Go for minimal PC polish**.

The V0.1 PC gameplay loop is technically playable end to end through the current shell path:

- match setup for 2, 3, and 4 local players;
- two-round match progression;
- Roll -> Selection/Reroll -> Action -> Resolution -> Reward -> Ranking -> Shop;
- shop purchase followed by the next round;
- final match completion and winner snapshot.

No blocking flow bug, crash, assertion, or state dead-end was found in the tested loop.

## Scenarios tested

Automated smoke added in this pass:

- `GrandpaGambit.PCShell.FullPlayableLoop2To4`

This smoke runs 2, 3, and 4 player matches with two configured rounds and verifies:

- round one starts after automatic Roll and reaches Selection/Reroll;
- lock, unlock, relock, and reroll preserve a locked die;
- Action accepts a direct no-target consumable and applies its effect;
- Resolution/Reward/Ranking produce score and ranking state;
- Shop exposes buyable offers;
- one shop purchase succeeds;
- a second purchase by the same player in the same shop is refused with `PurchaseAlreadyMade`;
- Shop completion advances to round two;
- final round Shop completion ends the match;
- final ranking contains all players and flags a winner.

Existing smoke coverage also exercised:

- `GrandpaGambit.PCShell.RoundHud.Commands`
- `GrandpaGambit.PCShell.RoundHud.ActionConsumableFeedback`
- `GrandpaGambit.PCShell.RoundHud.TargetSelectionFeedback`
- `GrandpaGambit.PCShell.RoundHud.ResolutionFeedback`
- `GrandpaGambit.PCShell.RoundHud.RewardRankingFeedback`
- `GrandpaGambit.PCShell.RoundHud.ShopPurchase`
- `GrandpaGambit.PCShell.ShopContinueFlow`
- `GrandpaGambit.TargetSelection.DieOptionsAndCancel`
- `GrandpaGambit.TargetSelection.RoundFlow.ConfirmedOpponentConsumable`

Manual PIE was not used as the primary evidence because the automated smoke now covers the full loop and the command surface directly. The map still loaded during editor automation startup.

## Results by player count

| Players | Result | Notes |
| --- | --- | --- |
| 2 | Pass | Two rounds completed; purchase and second-purchase refusal verified; final winner snapshot produced. |
| 3 | Pass | Same full-loop smoke completed with all ranking/final-state checks. |
| 4 | Pass | Same full-loop smoke completed with all ranking/final-state checks. |

## Bugs found and corrected

No blocking gameplay flow bug was found.

Corrected in this pass:

- Added missing automated coverage for the complete 2/3/4 player V0.1 PC loop, including the second round and final winner.

## Bugs remaining

Non-blocking issues observed:

- Editor automation startup logs a legacy `Gambit.umap` Blueprint load issue: a `Create Widget` node has no widget class and a missing debug widget dependency is referenced (`WBP_DebugMatchInspector`). The current PC shell does not depend on this path because `AGambitPlayerController` creates `UGambitPCShellWidget` through the C++ fallback. This should be cleaned before a content freeze, but it did not block the tested playable loop.
- The evidence is technical automation rather than visual/manual UI polish validation. Visual polish, layout, hover/focus, and click ergonomics remain the next pass.

## Go/no-go

Decision: **Go** for minimal PC polish.

Recommended next priority: minimal PC shell polish and visual/manual verification from `Content/GrandpaGambit/Maps/Gambit.umap`, while keeping the new full-loop smoke in the validation suite.
