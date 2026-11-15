# libSceNpGameIntent

## remove [libSceNpGameIntent additional exports]

## remove [sceNpGameIntentGetUdsNpComId, sceNpGameIntentLaunchApp, sceNpGameIntentLaunchApp2 and sceNpGameIntentNotifyAppLaunched]

## add Unknown function info

- [libSceNpGameIntent additional exports](../../../core/libs/prx/libSceNpGameIntent/Export.cpp) - these name/NID pairs are present in the inspected export inventory. Parameter lists, return types and behavior are unverified. Parameterless `int` declarations are import-resolution placeholders, not verified ABI prototypes; every entry returns `NpGameIntentErrorNotInitialized` (0x80553802) before initialization and throws through `NotImplemented_nid_no_patch` after initialization. No runtime functionality is implemented.

The inventory label is not authenticated retail-firmware provenance. The inspected image contains raw module-parameter words `0x13590001` at +0x10 and `0x13600007` at +0x14. Using the [published static ELF membership procedure](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298), its 20 unique function NIDs have sorted-LF SHA-256 `04952ce9705980433d9f6f51c9e20d2f6b354a102c085dd3b545ed0b96f5b6f7`; all 5 entries below are present. This digest identifies the function-NID set, not the whole binary or its ABI. An independent review on [this PR](https://github.com/boykopovar/AnyPS5/pull/11956#pullrequestreview) corroborated these identities using a separately reported 13.42 reference; that does not establish matching binaries, prototypes or behavior.

| Name | NID |
| --- | --- |
| `sceNpGameIntentGetPermittedIntentTypes` | `+u0P5feg100` |
| `sceNpGameIntentLaunchAppForPinvoke` | `4hjt8Rxl9CY` |
| `sceNpGameIntentLaunchAppRequestToGkp` | `hSVuTA3vJuQ` |
| `sceNpGameIntentRemoveLaunchCallback` | `RQPgtaOJdTk` |
| `sceNpGameIntentRequest` | `aScB-WDl+L0` |

- [sceNpGameIntentGetUdsNpComId, sceNpGameIntentLaunchApp, sceNpGameIntentLaunchApp2, sceNpGameIntentNotifyAppLaunched, sceNpGameIntentGetPermittedIntentTypes, sceNpGameIntentLaunchAppForPinvoke, sceNpGameIntentLaunchAppRequestToGkp, sceNpGameIntentRemoveLaunchCallback and sceNpGameIntentRequest](../../../core/libs/prx/libSceNpGameIntent/Export.cpp) (libSceNpGameIntent) - their pre-initialization return of NpGameIntentErrorNotInitialized (0x80553802) is unmeasured and follows the two traced GameIntent exports by analogy; after initialization, calls continue throwing through NotImplemented_nid_no_patch. Names and NIDs are present in the export inventory and [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNpGameIntent.c). They are declared without parameters solely to resolve known imports and make no ABI claim.
