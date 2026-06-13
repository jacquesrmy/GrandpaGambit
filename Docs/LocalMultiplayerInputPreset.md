# Local Multiplayer Input Preset

## Core Input Actions
Create these `Input Action` assets (Boolean value type):

- `IA_JoinLocalPlayer`
- `IA_LeaveLocalPlayer`
- `IA_Ready`
- `IA_Reroll`
- `IA_Consumable1`
- `IA_Consumable2`
- `IA_Consumable3`
- `IA_ShopOffer1`
- `IA_ShopOffer2`
- `IA_ShopOffer3`
- `IA_LockDie1`
- `IA_LockDie2`
- `IA_LockDie3`
- `IA_LockDie4`
- `IA_LockDie5`
- `IA_LockDie6`

## Mapping Contexts
Create these `Input Mapping Context` assets:

- `IMC_GlobalRoster`
- `IMC_Player0_Keyboard`
- `IMC_Player1_Gamepad`
- `IMC_Player2_Gamepad`
- `IMC_Player3_Gamepad`

### IMC_GlobalRoster
- `IA_JoinLocalPlayer` -> `Enter`
- `IA_LeaveLocalPlayer` -> `BackSpace`
- `IA_JoinLocalPlayer` -> `Gamepad Special Right`
- `IA_LeaveLocalPlayer` -> `Gamepad Special Left`

### IMC_Player0_Keyboard
- `IA_Ready` -> `SpaceBar`
- `IA_Reroll` -> `R`
- `IA_Consumable1` -> `1`
- `IA_Consumable2` -> `2`
- `IA_Consumable3` -> `3`
- `IA_ShopOffer1` -> `F1`
- `IA_ShopOffer2` -> `F2`
- `IA_ShopOffer3` -> `F3`
- `IA_LockDie1` -> `Q`
- `IA_LockDie2` -> `W`
- `IA_LockDie3` -> `E`
- `IA_LockDie4` -> `A`
- `IA_LockDie5` -> `S`
- `IA_LockDie6` -> `D`

### IMC_Player1_Gamepad / IMC_Player2_Gamepad / IMC_Player3_Gamepad
- `IA_Ready` -> `Gamepad Right Thumbstick Button`
- `IA_Reroll` -> `Gamepad Face Button Bottom`
- `IA_Consumable1` -> `Gamepad Face Button Left`
- `IA_Consumable2` -> `Gamepad Face Button Top`
- `IA_Consumable3` -> `Gamepad Face Button Right`
- `IA_ShopOffer1` -> `Gamepad Left Trigger`
- `IA_ShopOffer2` -> `Gamepad Right Trigger`
- `IA_ShopOffer3` -> `Gamepad Left Thumbstick Button`
- `IA_LockDie1` -> `Gamepad D-Pad Up`
- `IA_LockDie2` -> `Gamepad D-Pad Right`
- `IA_LockDie3` -> `Gamepad D-Pad Down`
- `IA_LockDie4` -> `Gamepad D-Pad Left`
- `IA_LockDie5` -> `Gamepad Left Shoulder`
- `IA_LockDie6` -> `Gamepad Right Shoulder`

## Input Config Data Asset
Create one `GambitLocalPlayerInputConfig` Data Asset:

- `DA_LocalInput_4P`

Configure:

- Shared contexts: `IMC_GlobalRoster` (priority 100)
- Player 0 contexts: `IMC_Player0_Keyboard` (priority 0)
- Player 1 contexts: `IMC_Player1_Gamepad` (priority 0)
- Player 2 contexts: `IMC_Player2_Gamepad` (priority 0)
- Player 3 contexts: `IMC_Player3_Gamepad` (priority 0)

## Notes
- If this data asset is not assigned, the code auto-builds the same mapping at runtime.
- If assigned, controller action fields can stay empty because fallback action binding is still available.
