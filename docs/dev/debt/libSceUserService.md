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

### Implemented integer settings

- [ReadUserIntegerSetting and WriteUserIntegerSetting](../../../core/libs/prx/libSceUserService/Export.cpp) - 382 scalar setting exports read and write per-user values using keys and defaults recovered from the firmware 13.60 `libSceUserService.sprx` in the local Ghidra project `Userservice-13.60`. In `ghidra-current-libSceUserService-triage.log`, setter NIDs `lAR1nkEoMBo`, `ZfUouUx2h8w` and `dCdhOJIOtR4` at `0x5f20`, `0x5ee0` and `0x5ea0` dispatch queries `0x500`, `0x501` and `0x502`; these sites establish the call arguments and query IDs, not the backing Registry persistence. The per-user key/default table was recovered from the firmware module's Registry access paths; the firmware dump and Ghidra database are local research artifacts and are not attached. Defaults for settings without a traced initial Registry value remain AnyPS5 model values. AnyPS5 keeps changes in process memory, not across restarts.

### Implemented string settings

- [ReadUserStringSetting and WriteUserStringSetting](../../../core/libs/prx/libSceUserService/Export.cpp) - 47 string setting exports use per-user storage keyed by firmware 13.60 Registry entry IDs and recovered field sizes from `libSceUserService.sprx` in the local `Userservice-13.60` Ghidra project. The module's Registry access paths provide the key and field-width evidence; AnyPS5's empty string for a field without an initial value is a host default and is not verified as the console default. The firmware dump and Ghidra database are local research artifacts and are not attached. Changes stay in process memory.

### Implemented binary settings

- [ReadUserDataSetting and WriteUserDataSetting](../../../core/libs/prx/libSceUserService/Export.cpp) - 36 fixed-size binary setting exports use per-user storage with field sizes and query IDs recovered from firmware 13.60 `libSceUserService.sprx` in the local `Userservice-13.60` Ghidra project. The module's query dispatches and Registry access paths establish the copied widths and IDs; they do not establish AnyPS5's zero-filled value for an unset field, which is a host default. The firmware dump and Ghidra database are local research artifacts and are not attached. Changes stay in process memory.

### Unknown function info

- [sceUserServiceSetPadProfileDataHuntList and sceUserServiceGetPadProfileDataHuntList](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the 13.60 managed wrappers recover an integer user ID, a profile-array pointer, and two 32-bit size or mode arguments; the profile element layout and behavior remain unknown.
- [sceUserServiceGetMicLevel and sceUserServiceSetMicLevel](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the 13.60 managed wrappers pass two by-reference values; their concrete layouts and host microphone behavior are not recovered.
- [sceUserServiceSetVolumeForController and sceUserServiceGetVolumeForController](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the 13.60 managed wrappers recover `(int userId, uint16_t deviceType, uint16_t deviceIndex, int value)` and the corresponding `int32_t*` getter; controller volume state is not modeled.
- [sceUserServiceGetPadSpeakerVolume and sceUserServiceSetPadSpeakerVolume](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the 13.60 managed wrappers recover `(int userId, uint16_t deviceType, uint16_t deviceIndex, int32_t* value)` and the corresponding `int value` setter; controller speaker state is not modeled.
- [sceUserServiceSetVolumeForGenericUSB and sceUserServiceGetVolumeForGenericUSB](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the 13.60 managed wrappers recover `(int userId, uint16_t deviceType, uint16_t deviceIndex, int value)` and the corresponding `int32_t*` getter; host audio-device volume state is not modeled.
- [sceUserServiceGetAppearOfflineSetting and sceUserServiceSetAppearOfflineSetting](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the 13.60 managed wrappers recover `(int userId, int32_t* value)` and `(int userId, int32_t value)`; the native exports dispatch through an unavailable backend, so the behavior remains unimplemented.
- [sceUserServiceUpdateCronosCountryList](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - firmware 13.60 takes a country-list string and dispatches operation `0x3002a`; the behavior belongs to the unavailable UserService backend, so the export remains unimplemented and throws.
- [sceUserServiceGetLoginUserIdList and sceUserServiceGetEvent for user 0](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - under ANYPS5_PAD_PORT0_STATE=controller user 0 logs out when its controller is removed and logs in when it returns, after a hold of 500 ms that AnyPS5 chooses. Whether the console logs the initial user out on controller removal, how long it takes and which event it sends are not measured. The default mode keeps user 0 logged in as before.
- [sceUserServiceGetNpAccountId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the account id for a logged-in user with no linked PSN account is assumed to be 0 with success; libSceNpManager's `sceNpGetAccountIdA` answers the same question with `SCE_NP_ERROR_SIGNED_OUT`. Console measurement on PS4 Pro firmware 10.71 confirms a user that is not logged in returns `0x80960105` (`USER_SERVICE_ERROR_UNKNOWN_USER`) and leaves the output untouched, differing from `sceUserServiceGetUserName`.
- [Older settings exports](../../../core/libs/prx/libSceUserService/LegacyExports.cpp) - Forty callable NIDs are declared as throwing placeholders for older software. Their signatures and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. The corresponding NIDs are absent from the later version-labelled inventory. Representative later settings functions use a related dispatch pattern, which does not establish compatibility for these settings. No functionality is implemented.

- [sceUserServiceInitialize and sceUserServiceInitialize2](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - event delivery has no background service thread, so the confirmed priority and Initialize2 CPU affinity mask do not affect host scheduling. Of 23 `sceUserServiceInitialize` calls in ten titles checked, 17 pass no param, four pass priority 700 (Borderlands 4 PPSA01494, Crimson Desert PPSA25286, MXGP 26 PPSA30839, Super Meat Boy 3D PPSA31162), one passes 767 (Ghost of Tsushima PPSA02225) and one in Metal Gear Solid 4's `mgs4.elf` passes a param from a register. None calls `sceUserServiceInitialize2`.
- [sceUserServiceGetUserColor](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the profile colour corresponds to user index (0: blue, 1: red, 2: green, 3: pink); colours 1, 2 and 3 are assumed, not measured
- [sceUserServiceGetUserName](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - `0x80960105`, which a PS5 (firmware 13.60) returns for a user id it does not know, has no known name; it is `USER_SERVICE_ERROR_UNKNOWN_USER` here. On the console a registered user who is not logged in still gets their name; here up to 4 users ("Player", "Player2", "Player3", "Player4") are supported; user names "Player2", "Player3" and "Player4" are assumed, not measured
- [sceUserServiceGetEvent](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - login and logout events queued dynamically on controller hotplug have no source and are assumed, not measured. Six titles disassembled for this entry drain the queue until the call returns nonzero. PPSA01494 01.100.002 (at 0x374C0C6), PPSA30839 01.003.000 (0x4DF3F0), PPSA31162 02.000.001 (0x550ED0) and PPSA34502 01.010.000 (0x30035C, 0x300433) act only on `event_type` 0 and 1, reading `user_id` at offset 4 for both, and skip other types. PPSA18194 01.004.003 (0x16D44A3) tells `NOT_INITIALIZED` (0x80960002) and `NO_EVENT` (0x80960007) apart and logs any other error. PPSA21564 01.007.000 copies each 8-byte event into a list at one site (0x2AFCA8) and discards them at the other (0x2B6CF3)
- [sceUserServiceGetAgeLevel](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - on a PS5 (firmware 13.60) the value is the age set in the user's parental restriction (0, 7, 12 and 15 were measured) and `0xFFFFFFFF` without one; a user who is not logged in returns `0x80960009` with or without an output. Parental restrictions are not modelled, so the initial user is always reported as unrestricted. PPSA12544 (Unity) only stores the value for its user profile. The one call in PPSA12544 (0x16a80a0), made only when bit 0x4 of its flags is set, writes the value to a field of its own object (offset 0x5170) and ends its chain of getters on any nonzero result; what it does with the value afterwards was not read.
- [sceUserServiceGetAccessibilityChatTranscription, PressAndHoldDelay, TriggerEffect, Vibration, ZoomEnabled, ZoomFollowFocus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - report 0, 0, 1, 1, 0 and 1, what a PS5 (firmware 13.60) reports for a user whose accessibility settings are all at their defaults; the console reports the user's own settings (a changed vibration intensity read 3), which are not modelled. What each number means is unknown. A user who is not logged in returns `0x80960009` with or without an output. One title (PPSA04354) disassembled for this entry calls all six in a row (first at 0xea0efa) with its user ID, only when a flag bit (0x10) is set; a result other than 0 from any of them ends the block and is returned, and only when all six return 0 does it store the six values in its object.
- [sceUserServiceGetAccessibilityKeyremapEnable, sceUserServiceGetAccessibilityZoom](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - values reported by a console and their formats are unmeasured, so both throw.
- [sceUserServiceGetCurrentUserGroupIndex](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetRegisteredHomeUserIdList](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetAudioFocus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - 13.60 ABI is `(int userId, void* audioFocus)` and dispatches query `0x3002d` with an 0x18-byte result; the result layout and backend behavior are not modeled, so the export still throws.
- [sceUserServiceGetIsCronosEnabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - reads a per-user Boolean setting (`0x8d8`, bit 1) in firmware 13.60. AnyPS5 has no Cronos settings store, so it reports false for a logged-in user; the value for Cronos-enabled users remains unverified.
- [sceUserServiceSetUserGroupName](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceSetSystemLoggerHashedAccountIdParam](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the 13.60 managed wrapper recovers `(int userId, uint8_t* value, int size, uint64_t flags)`; hashed-account parameter state is not modeled.
- [sceUserServiceGetUserGroupName](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetHomeDirectory](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetKratosPrimaryUser](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetSystemLoggerHashedAccountIdParam](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceSetUserGroupIndex](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceSetForegroundUser](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceSetGamePresets](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the 13.60 managed wrapper recovers an integer user ID and a by-reference preset structure; the structure layout and behavior remain unknown.
- [sceUserServiceGetUserWithHearingAssist](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetEventVsh](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetPlayingBigAppTitleId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetDefaultNewUserGroupName](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetDeletedUserInfo](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetSecureHomeDirectory](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetUserGroupNameList](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceSetUserName](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetUserGroupNum](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetIsSpartacusEnabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature
- [sceUserServiceGetKeyboardType, sceUserServiceGetJapaneseInputType, sceUserServiceGetTraditionalChineseInputType](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - AnyPS5 stores no per-user keyboard setting, so the initial user reports the keyboard type of the configured system language (`ANYPS5_LANGUAGE`, English US by default) and 0 for the two input variants, and any other user id returns `USER_SERVICE_ERROR_UNKNOWN_USER`. The language-to-keyboard-type table is the repository owner's (languages 0 to 29); a valid language outside it, such as Ukrainian (30), reports 20 (US English). A value that is not a console language number is rejected by `sceSystemServiceParamGetInt` with an error naming `ANYPS5_LANGUAGE`; the call throws it and leaves the output untouched. The input variant is 0 because the owner's table gives 0 for Japanese and Traditional Chinese, the only languages needing one. `sceConvertKeycodeGetImeKeyboardType` consumes these values.
- [sceUserServiceGetPlatformPrivacyWs1](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the platform privacy setting for an offline user without a linked PSN account is assumed to be 0 (not permitted) with success; actual console privacy querying is not modelled. Return of `0x80960009` (`USER_SERVICE_ERROR_NOT_LOGGED_IN`) with or without an output for a user who is not logged in is borrowed from `sceUserServiceGetAgeLevel` and is not measured for this call.
