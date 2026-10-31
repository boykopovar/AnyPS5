# libSceUserService technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceUserService/InventoryExports.cpp) - 36 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. None of these NIDs appears in the pinned public 3.20 catalogue for this library. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `sceUserServiceGetPartyChatTranscriptionVoiceType` | `Ebq9pcMYrko` |
| `sceUserServiceSetVoiceAgentEnableByButton` | `Jz06XYyyqGE` |
| `Unknown` | `-JCFmPNhxY8` |
| `Unknown` | `-dkPbHlL9O8` |
| `Unknown` | `-o+vKoB+bIw` |
| `Unknown` | `0rgUYD57RHc` |
| `Unknown` | `1b1BuBizZ2Q` |
| `Unknown` | `1fV-P4lyYAM` |
| `Unknown` | `6Qsib4RLbFw` |
| `Unknown` | `6erDJ4-MGCs` |
| `Unknown` | `A-wBTL+Cs5A` |
| `Unknown` | `BdzmN4JP3JM` |
| `Unknown` | `Bx+ESDBWJm4` |
| `Unknown` | `Cqt7pJZRKLI` |
| `Unknown` | `D5lYX9Wm7PI` |
| `Unknown` | `DL6cKVeISqs` |
| `Unknown` | `ErTMkmSho4E` |
| `Unknown` | `GbRykQBfmM8` |
| `Unknown` | `JD766DzZIIU` |
| `Unknown` | `L9NSPGwLZTQ` |
| `Unknown` | `PBJH7EFon8c` |
| `Unknown` | `UKInJ0LzusI` |
| `Unknown` | `WGYLbMB95kM` |
| `Unknown` | `YZwUXHtKrXI` |
| `Unknown` | `fPmaQlxVoqI` |
| `Unknown` | `hKzuaakSOLE` |
| `Unknown` | `iRkGgbFAhcw` |
| `Unknown` | `isbcQWjV-qU` |
| `Unknown` | `keFAPHk6pIc` |
| `Unknown` | `oF3Ygwz32Bs` |
| `Unknown` | `pfgU0SxMMxw` |
| `Unknown` | `pr3J39r9ArQ` |
| `Unknown` | `shuQwNy79HM` |
| `Unknown` | `tPbEIE0wYCM` |
| `Unknown` | `u5SR3+a0lTw` |
| `Unknown` | `ys8aAGjUads` |

### Unknown function info

- [sceUserServiceGetNpAccountId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the account id for a logged-in user with no linked PSN account is assumed to be 0 with success; libSceNpManager's `sceNpGetAccountIdA` answers the same question with `SCE_NP_ERROR_SIGNED_OUT`. Console measurement on PS4 Pro firmware 10.71 confirms a user that is not logged in returns `0x80960105` (`USER_SERVICE_ERROR_UNKNOWN_USER`) and leaves the output untouched, differing from `sceUserServiceGetUserName`.
- [sceUserServiceInitialize and sceUserServiceInitialize2](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - event delivery has no background service thread, so the confirmed priority and Initialize2 CPU affinity mask do not affect host scheduling. Of 23 `sceUserServiceInitialize` calls in ten titles checked, 17 pass no param, four pass priority 700 (Borderlands 4 PPSA01494, Crimson Desert PPSA25286, MXGP 26 PPSA30839, Super Meat Boy 3D PPSA31162), one passes 767 (Ghost of Tsushima PPSA02225) and one in Metal Gear Solid 4's `mgs4.elf` passes a param from a register. None calls `sceUserServiceInitialize2`.
- [sceUserServiceGetUserColor](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the profile colour corresponds to user index (0: blue, 1: red, 2: green, 3: pink); colours 1, 2 and 3 are assumed, not measured
- [sceUserServiceGetUserName](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - `0x80960105`, which a PS5 (firmware 13.60) returns for a user id it does not know, has no known name; it is `USER_SERVICE_ERROR_UNKNOWN_USER` here. On the console a registered user who is not logged in still gets their name; here up to 4 users ("Player", "Player2", "Player3", "Player4") are supported; user names "Player2", "Player3" and "Player4" are assumed, not measured
- [sceUserServiceGetEvent](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - login and logout events queued dynamically on controller hotplug have no source and are assumed, not measured. Six titles disassembled for this entry drain the queue until the call returns nonzero. PPSA01494 01.100.002 (at 0x374C0C6), PPSA30839 01.003.000 (0x4DF3F0), PPSA31162 02.000.001 (0x550ED0) and PPSA34502 01.010.000 (0x30035C, 0x300433) act only on `event_type` 0 and 1, reading `user_id` at offset 4 for both, and skip other types. PPSA18194 01.004.003 (0x16D44A3) tells `NOT_INITIALIZED` (0x80960002) and `NO_EVENT` (0x80960007) apart and logs any other error. PPSA21564 01.007.000 copies each 8-byte event into a list at one site (0x2AFCA8) and discards them at the other (0x2B6CF3)
- [sceUserServiceGetAgeLevel](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - on a PS5 (firmware 13.60) the value is the age set in the user's parental restriction (0, 7, 12 and 15 were measured) and `0xFFFFFFFF` without one; a user who is not logged in returns `0x80960009` with or without an output. Parental restrictions are not modelled, so the initial user is always reported as unrestricted. PPSA12544 (Unity) only stores the value for its user profile. The one call in PPSA12544 (0x16a80a0), made only when bit 0x4 of its flags is set, writes the value to a field of its own object (offset 0x5170) and ends its chain of getters on any nonzero result; what it does with the value afterwards was not read.
- [sceUserServiceGetAccessibilityChatTranscription, PressAndHoldDelay, TriggerEffect, Vibration, ZoomEnabled, ZoomFollowFocus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - report 0, 0, 1, 1, 0 and 1, what a PS5 (firmware 13.60) reports for a user whose accessibility settings are all at their defaults; the console reports the user's own settings (a changed vibration intensity read 3), which are not modelled. What each number means is unknown. A user who is not logged in returns `0x80960009` with or without an output. One title (PPSA04354) disassembled for this entry calls all six in a row (first at 0xea0efa) with its user ID, only when a flag bit (0x10) is set; a result other than 0 from any of them ends the block and is returned, and only when all six return 0 does it store the six values in its object.
- [sceUserServiceGetAccessibilityKeyremapEnable, sceUserServiceGetAccessibilityZoom](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - values reported by a console and their formats are unmeasured, so both throw.
- [sceUserServiceGetKeyboardType, sceUserServiceGetJapaneseInputType, sceUserServiceGetTraditionalChineseInputType](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - AnyPS5 stores no per-user keyboard setting, so the initial user reports the keyboard type of the configured system language (`ANYPS5_LANGUAGE`, English US by default) and 0 for the two input variants, and any other user id returns `USER_SERVICE_ERROR_UNKNOWN_USER`. The language-to-keyboard-type table is the repository owner's (languages 0 to 29); a valid language outside it, such as Ukrainian (30), reports 20 (US English). A value that is not a console language number is rejected by `sceSystemServiceParamGetInt` with an error naming `ANYPS5_LANGUAGE`; the call throws it and leaves the output untouched. The input variant is 0 because the owner's table gives 0 for Japanese and Traditional Chinese, the only languages needing one. `sceConvertKeycodeGetImeKeyboardType` consumes these values.
- [sceUserServiceGetPlatformPrivacyWs1](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the platform privacy setting for an offline user without a linked PSN account is assumed to be 0 (not permitted) with success; actual console privacy querying is not modelled. Return of `0x80960009` (`USER_SERVICE_ERROR_NOT_LOGGED_IN`) with or without an output for a user who is not logged in is borrowed from `sceUserServiceGetAgeLevel` and is not measured for this call.
