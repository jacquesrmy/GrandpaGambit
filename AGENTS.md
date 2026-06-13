# AGENTS.md

## Purpose

This repository contains an Unreal Engine 5 C++ local multiplayer party game project for 2 to 4 players.

The goal of this document is to guide any coding agent working on the project so that all generated code, systems, files, and architecture remain:

- clean
- modular
- scalable
- reusable
- easy to maintain
- easy to configure from Unreal Editor / Project Settings / Blueprint defaults where appropriate
- free from spaghetti dependencies

This is not a prototype-trash project.  
This project must be structured like a real production-ready foundation for a reusable and scalable game codebase.

---

## High-level game overview

The game is a **local multiplayer turn-based party game** for **2 to 4 players**.

Core design pillars:

- accessible and readable
- fast rounds
- controlled randomness
- build creation through dice / passive modules / consumables
- strategic choices through rerolls, economy, shop, and shared pool
- limited negative interaction between players
- clean onboarding through an interactive table/lobby setup

### Core fantasy

Players begin seated around a realistic calm table environment where they can casually interact with dice and discover the basics through something close to classic Yahtzee.

When the actual match starts, the game shifts abruptly into a chaotic futuristic version of the scene:

- more dynamic UI
- stronger VFX
- faster pacing
- advanced gameplay systems activated

This contrast is central to the identity of the game.

---

## Core gameplay loop

A full match contains multiple rounds.

At the end of each round, players are ranked and receive victory points.  
The final winner is the player with the highest total victory points after the configured number of rounds.

### Round phases

Each round follows this exact ordered flow:

1. **Roll Phase**
2. **Selection / Reroll Phase**
3. **Action Phase**
4. **Resolution Phase**
5. **Reward Phase**
6. **Ranking Phase**
7. **Shop Phase**

### Details of each phase

#### 1. Roll Phase
Each player rolls a number of dice.

- Default dice count: 4
- Dice count may later be modified by upgrades or items
- Dice can be of different types in the future

#### 2. Selection / Reroll Phase
Players can optimize their results.

- lock selected dice
- reroll unlocked dice
- up to two rerolls by default
- reroll limits should be configurable

#### 3. Action Phase
Players may optionally use consumables.

Consumables can:

- alter dice values
- modify resolution
- interact with opponents
- protect against negative effects
- create temporary bonuses

Consumable usage is always limited and rule-driven.

#### 4. Resolution Phase
The system automatically computes the best score available for each player based on:

- dice combination achieved
- base combination rules
- active module effects
- consumable effects
- additive bonuses
- multiplicative bonuses
- caps / diminishing returns

This phase must be deterministic and readable.

#### 5. Reward Phase
Players receive round rewards.

- score result is finalized
- gold is granted based on performance
- economy rules apply
- interest may apply

#### 6. Ranking Phase
Players are ranked by round score.

Default victory point distribution:

- 1st: 5 VP
- 2nd: 3 VP
- 3rd: 2 VP
- 4th: 1 VP

This structure must stay data-driven.

#### 7. Shop Phase
Each player opens an individual shop choice flow.

- choose 1 item among 3 offers by default
- item offers come from controlled pools
- some items are shared/limited globally
- purchases affect future rounds

---

## Main game systems

### Dice system
Each player owns a dice pool / loadout.

- default active dice count: 4
- future support for additional dice
- different dice types may exist
- some dice may transform outcomes or add effects

The dice system must not be hardcoded around one single die implementation.

### Modules
Modules are passive persistent gameplay modifiers.

Examples:

- additive bonuses
- multipliers
- dice transformations
- reroll modifications
- synergy effects
- economy modifiers
- scoring modifiers

Constraints:

- limited active module slots per player
- default target: 4 to 5 active modules

Modules must be composable and data-driven.

### Consumables
Consumables are activatable items used during rounds.

They should support:

- immediate effects
- temporary effects
- self-target
- opponent-target
- defensive use
- round-limited logic

Consumable effects must not be embedded in UI or phase orchestration code.

### Economy
Players gain gold after rounds.

Rules:

- max gold: 50 by default
- interest: +1 gold per full 10 gold saved
- max interest: +5 by default

Economy values must be configurable.

### Shared pool
Some modules and dice are globally limited in stock.

When one player acquires a shared item, it is no longer available to others.

This system exists to:

- force build variety
- reduce dominant strategies
- create indirect competition

This must be handled by a dedicated ownership/availability system, not by ad hoc checks inside shop UI.

### Scoring
Scoring is based on dice combinations inspired by Yahtzee, then modified by gameplay systems.

Possible inputs:

- base combination
- sum of dice
- additive modifiers
- multipliers
- triggered effects
- protection rules
- caps and diminishing returns

Scoring must be isolated in dedicated systems so balancing can happen without touching phase or UI code.

### Multiplayer model
This game is currently **local multiplayer only**.

Assumptions:

- 2 to 4 local players
- turn-based / phase-based
- no online networking now

However, architecture should avoid choices that make future network migration impossible.  
Do not hardwire everything to one screen-only prototype flow.

### Lobby / onboarding
Before the match starts, players sit around the table in an interactive onboarding scene.

Goals:

- natural discovery of base mechanics
- no heavy tutorial
- social and readable setup
- transition into main match scene tone

This onboarding should remain separated from core match systems.

---

## Architecture philosophy

### Main principles

All generated code must obey these rules:

1. **Single responsibility first**
   - one class = one main reason to change

2. **Composition over monoliths**
   - prefer ActorComponents / UObject helpers over huge god classes

3. **Data-driven design**
   - static tunable content should live in DataAssets / DataTables / config-friendly structures
   - avoid hardcoded gameplay constants in business logic

4. **Clear separation of concerns**
   - gameplay rules
   - state ownership
   - presentation/UI
   - content definitions
   - orchestration
   - persistence/config

5. **No hidden cross-system spaghetti**
   - core systems should not depend directly on each other unnecessarily
   - communicate through interfaces, events, owned components, or narrowly scoped APIs
   - avoid circular dependencies
   - avoid “manager knows everything and does everything”

6. **Private by default**
   - class members private unless there is a real reason otherwise
   - use protected only for extension
   - expose with getters/setters or explicit UFUNCTIONs where useful

7. **Blueprints are integration/presentation helpers, not the core logic home**
   - core rules in C++
   - Blueprint exposure only where it improves usability

8. **Editor-friendly**
   - everything that designers need to tweak should be exposed properly
   - use appropriate specifiers: EditDefaultsOnly, EditAnywhere, BlueprintReadOnly, etc.
   - prefer project settings / config objects / data assets for global values

9. **Simple extensibility**
   - adding a new die/module/consumable should not require modifying many unrelated files

10. **Future-safe**
   - keep compatibility in mind for future online/network adaptation
   - avoid assumptions that all state only exists in widgets

---

## Required code organization

Use a clear source tree.  
Suggested structure:

```text
Source/<ProjectName>/
    Core/
        Types/
        Interfaces/
        Settings/
        Logging/
        Utils/

    Game/
        Modes/
        States/
        Flow/
        Rules/

    Players/
        Controllers/
        States/
        Pawns/
        Components/

    Dice/
        Actors/
        Components/
        Evaluation/
        Data/

    Items/
        Base/
        Modules/
        Consumables/
        DiceItems/
        Effects/
        Data/

    Scoring/
        Evaluators/
        Calculators/
        Models/

    Economy/
        Components/
        Models/

    Shop/
        Components/
        Models/
        Data/

    Ranking/
        Components/
        Models/

    UI/
        Widgets/
        ViewModels/
        HUD/

    Data/
        Assets/
        Tables/
        Definitions/

    Managers/
        Round/
        Match/
        SharedPool/

    Debug/

Folder rules
Do not dump everything into one folder.
Keep systems grouped by domain.
Shared enums/structs should live in a stable common location.
Avoid deep dependency chains across unrelated folders.
A folder should represent a domain, not a random technical accident.
Unreal class usage rules

Use the right Unreal type for the right responsibility.

Use AActor only when:
there is world presence
there is transform/spatial meaning
there is physical interaction
replication/world lifecycle matters directly
Use UActorComponent when:
behavior must attach to Actor-based owners
logic is reusable across several actor types
the system is part of a PlayerState / Pawn / GameState / GameMode actor
Use UObject when:
no transform needed
pure gameplay logic / rule processing / effect evaluation
helper evaluators / calculators / definitions
Use UDataAsset when:
static game content definitions are needed
item definitions must be authorable in editor
modular content authoring is required
Use USTRUCT when:
serialized lightweight data is needed
result payloads or definition records are needed
state snapshots / shop offers / score outputs are needed
Use UENUM when:
a controlled set of states or categories is needed
Use interfaces when:
multiple systems can respond to the same interaction pattern
item effects or phase callbacks need polymorphism without class spaghetti
Project-level configuration philosophy

All configs should be easy to use and clean.

Prefer configuration in:
Project Settings
Developer Settings classes
DataAssets
defaults in Blueprint child classes
config-backed gameplay settings objects where appropriate
Avoid:
magic numbers buried in CPP files
duplicated constants across systems
forcing users to recompile code to tweak balancing values
per-widget gameplay constants
Good candidates for global settings

Create project settings / developer settings for values like:

default player count limits
default round count
default active dice count
reroll limit
module slot count
consumable slot count
max gold
interest interval
max interest
default victory point table
shop offer count
rarity weights
debug toggles

Use editor-facing settings cleanly.
Only special-case configs should remain code-side.

System ownership model

Ownership must be explicit and stable.

GameMode

Responsible for authoritative match flow in the current local setup.

Should manage:

match start/end
round lifecycle transitions
high-level orchestration

Should not contain all detailed gameplay logic.

GameState

Responsible for globally visible runtime state.

Should hold:

current round number
current phase
round-related public state
player ranking snapshot
shared pool state if globally visible
PlayerState

Responsible for persistent per-player match state.

Should hold:

gold
victory points
current round score
owned dice/loadout
owned modules
consumables inventory
shop-related player data if needed
PlayerController

Responsible for player commands and input routing.

Should not own core scoring/economy rules.

Components

Reusable systems should live in components when ownership is actor-based, such as:

inventory
economy
dice control
shop interaction
round participation hooks
UObject services

Pure rules systems should live in objects/services, such as:

score calculation
combination evaluation
item effect resolution
shop roll generation
ranking calculation
Required core systems and responsibilities

Below is the expected clean separation.

1. Match / round flow system

Responsible for phase progression and round orchestration.

Must:

know current phase
validate phase transitions
trigger start/end hooks for phases
coordinate player readiness if needed
stay thin and orchestration-focused

Must not:

directly implement all scoring logic
directly implement all shop logic
directly implement all item effects
2. Dice system

Responsible for:

rolling dice
locking/unlocking dice
rerolling unlocked dice
exposing current results
managing dice instances/types if needed

Dice evaluation should remain separate from pure scoring logic.

3. Combination evaluation system

Responsible for:

reading final dice values
detecting combinations
selecting best matching combination
producing structured evaluation output

Must be deterministic and testable.

4. Score calculation system

Responsible for:

converting evaluation output into final score
applying additive bonuses
applying multipliers
applying caps/diminishing returns
producing detailed score breakdowns
5. Inventory / loadout system

Responsible for:

owned dice
active modules
consumables
slot constraints
add/remove/equip usage rules
6. Economy system

Responsible for:

gold gain
spending
cap enforcement
interest computation
7. Shop system

Responsible for:

generating offers
validating availability
reading shared pool
pricing
purchase flow

Should not be driven by UI-specific logic.

8. Shared pool system

Responsible for:

global stock tracking
reserving/removing limited items
availability queries
9. Ranking system

Responsible for:

sorting players by round score
assigning victory points
producing round ranking output
10. Item/effect system

Responsible for:

common item definitions
passive module effects
consumable effects
die-related effects
effect execution contracts

Effects must be extendable without rewriting core match flow.

Dependency rules

These rules are critical.

Allowed direction of dependency

Lower-level systems should not depend on higher-level presentation systems.

Preferred direction:

Core Types / Data / Definitions
Pure Gameplay Services
State Owners
Orchestration Layer
UI / Presentation
Explicit anti-spaghetti rules
Do not:
make UI widgets calculate gameplay truth
let shop widgets modify shared pool directly
let PlayerController own economy rules
let GameMode become a 3000-line god object
let item subclasses directly query half the project
place combination scoring inside dice actor visuals
hard-reference many unrelated systems in every file
Prefer:
narrow interfaces
event/delegate based hooks where appropriate
service-style calculators/evaluators
data passed through structs
explicit ownership boundaries
Coding rules
Naming
Follow Unreal naming conventions strictly
A for Actor classes
U for UObject / Component / Widget
F for structs
E for enums
I for interfaces
booleans prefixed with b
Visibility
private by default
protected only for intended subclass extension
public only for stable API surface
UPROPERTY / UFUNCTION

Use only when required.

Preferred exposure rules:

VisibleAnywhere for runtime-visible owned references
EditDefaultsOnly for class default tuning
EditAnywhere only when instance tuning is actually needed
BlueprintReadOnly by default over BlueprintReadWrite
BlueprintCallable only for real interaction entry points
avoid excessive Blueprint exposure
Includes
minimal includes
forward declarations whenever possible in headers
heavy includes in cpp files
Functions
short
explicit
focused
avoid giant multi-purpose functions
Comments
comment intent, not the obvious
explain non-trivial decisions
avoid noise comments
Data-driven content rules

Content should be easy to author and balance.

Use DataAssets / definitions for:
dice definitions
module definitions
consumable definitions
rarity metadata
shop offer data
scoring table values when useful
Static definition objects should contain:
display name
rarity
cost
tags
effect metadata
editor-readable description
icon references if needed later
balance values
Runtime state should not live inside static assets

Keep runtime mutable state in player-owned or match-owned structures.

Phase system rules

The round phase system must be explicit and strongly typed.

Use a dedicated enum such as:

None
Roll
Reroll
Action
Resolution
Reward
Ranking
Shop
RoundEnd

Rules:

transitions must be validated centrally
phase entry/exit should be eventable
phase-specific behavior should be delegated to dedicated systems
avoid string/state-name logic

A simple state machine is preferred over hidden boolean combinations.

Item architecture rules

Items must be extensible without creating hardcoded branching everywhere.

Base item categories

At minimum support:

Dice-related items
Passive modules
Consumables
Recommended split
base item definition class
runtime item instance/state if needed
effect objects or effect descriptors
category-specific subclasses only where behavior differs meaningfully
Avoid
giant switch statements on item name/id in core systems
one item manager with all behavior hardcoded
embedding effect logic in UI
Blueprint usage rules

Blueprints are welcome for:

presentation
animations
scene setup
HUD/widget composition
derived content classes
light interaction glue

Blueprints are not the source of truth for:

round rules
score logic
economy logic
shared pool logic
ranking logic

If core gameplay must be debuggable and scalable, it belongs in C++.

Scalability rules

The project must be easy to extend in these future directions:

new dice types
new module categories
new consumable effects
more score modifiers
better balancing tools
AI or solo mode later
online/network adaptation later
alternate game modes later
expanded shop rules
event/relic/challenge systems later

When adding a feature, prefer extending data + isolated services rather than modifying core orchestration everywhere.

Local multiplayer constraints

Current target is local multiplayer only.

Important implications:

input routing must support multiple local players cleanly
player state must still be separated per player
UI should not assume one-player-only ownership
avoid singleton shortcuts that break multi-player state separation

Even if networking is not active now, avoid architecture that would make per-player authority impossible later.

What coding agents should generate

When implementing systems, agents should produce:

a clear folder placement
a focused class with one main responsibility
header and cpp with meaningful base implementation
proper specifiers and access control
integration points with the rest of the architecture
minimal but usable APIs
no dummy empty shells unless explicitly asked
no hidden hardcoded coupling

For every new class, the agent should be able to explain:

why it exists
who owns it
what data it owns
what it does not own
what systems are allowed to call it
how it can evolve later
What coding agents must avoid

Do not generate:

giant all-in-one manager classes
unnecessary inheritance depth
random static helper dumping grounds
editor-unfriendly hardcoded constants
UI-driven gameplay truth
duplicated enums/structs in many places
circular dependencies
placeholder architecture with no practical use
“tutorial style” toy code pretending to be production architecture
Expected initial implementation targets

The first real architecture pass should include at least:

match game mode
game state
player state
player controller
round flow manager/component
dice component / dice rolling support
inventory/loadout component
economy component
shop manager/component
shared pool manager
score calculator
combination evaluator
ranking manager
item base definitions
module definitions
consumable definitions
dice definitions
core enums and structs
settings/config objects

These should form a usable foundation, not just empty files.

Round flow reference

A coding agent should preserve the following canonical round flow:

start round
reset temporary round state
roll active dice for each player
allow lock selection
allow rerolls within allowed limit
allow consumable usage during action window
finalize dice state
evaluate best combination
calculate final score with modifiers
assign gold reward
compute ranking
assign victory points
open shop choices
process purchases
prepare next round
if final round reached, end match
Inspirations and intended feel

These references inform the design intent, not direct copying:

Balatro for build synergies and multipliers
Teamfight Tactics for economy, rarity, and long-term decision making
Mario Party for accessibility and local multiplayer chaos
Yahtzee for readable combination-based dice scoring

The project should preserve readability and fun first, but with meaningful build depth.

Final instruction to coding agents

When in doubt, choose the solution that is:

simplest without being simplistic
modular without being over-engineered
data-driven without becoming abstract nonsense
easy to configure in Unreal Editor
easy to extend without rewriting core systems

Every major system must be able to stand on its own with a clean API.
Core components should not be entangled inside the same files or depend on each other in spaghetti ways.