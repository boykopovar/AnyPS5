# libSceUpdateService technical debt

### Unknown function info

- [Additional libSceUpdateService exports](../../../core/libs/prx/libSceUpdateService/Export.cpp) - the name/NID pairs below are defined exports in the [firmware 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceupdateservice-b7f2a7b06c.jsonl). Their ABIs and behaviour are unknown. Parameterless `int` declarations are import-resolution placeholders, not established signatures; every call throws `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `sceUpsrvFlushHttpCookie` | `9SVS4rCdJvo` |

- [sceUpsrvGetCompletedTaskId, sceUpsrvGetRebootFlag, sceUpsrvGetSystemExUpdateTaskState, sceUpsrvInitialize, sceUpsrvPostErrorLog, sceUpsrvResumePausedTask, sceUpsrvSetFakeVersion, sceUpsrvStartUpdateSession, sceUpsrvSwitchSystemExBank, sceUpsrvSwitchSystemExBankWithoutVerify, sceUpsrvTerminate, sceUpsrvUpdateCancelBDUpdate, sceUpsrvUpdateCheckCancel, sceUpsrvUpdateCheckCreateHandler, sceUpsrvUpdateCheckDestroyHandler, sceUpsrvUpdateCheckDoCheckSystem, sceUpsrvUpdateCheckDoCheckSystemForDevKit, sceUpsrvUpdateCheckDoCheckSystemForSettings, sceUpsrvUpdateCheckDoCheckSystemWithPupInfo, sceUpsrvUpdateCreateUpdateTask, sceUpsrvUpdateDestroyUpdateTask, sceUpsrvUpdateDoCheckBootUpdate, sceUpsrvUpdateDoUpdateSystemEx, sceUpsrvUpdateDoUpdateWithEntitlement, sceUpsrvUpdateDoUpdateWithPupInfo, sceUpsrvUpdateDoUpdateWithPupPath2, sceUpsrvUpdateGetBetaAgreementUrl, sceUpsrvUpdateGetEulaDisplay, sceUpsrvUpdateGetEulaStr, sceUpsrvUpdateGetEulaStrLen, sceUpsrvUpdateGetFeatureListUrl, sceUpsrvUpdateGetFeatureListUrlForSystemExUpdate, sceUpsrvUpdateGetFeatureListUrlForUpdate, sceUpsrvUpdateGetImageWritingProgress, sceUpsrvUpdateGetProgress, sceUpsrvUpdateGetUpdateTaskInfo, sceUpsrvUpdateGetUpdateTaskMgrInfo, sceUpsrvUpdateSetEulaAccept, sceUpsrvUpdateStartUpdateTask and sceUpsrvVerifySystemExBank](../../../core/libs/prx/libSceUpdateService/Export.cpp) (libSceUpdateService) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceUpdateService.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceUpdateService/Export.cpp) - 15 entries observed in a 3.20-labelled image and a later 13.60-labelled image are import-resolution declarations only. Neither image label authenticates a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/752e1b1d4a6ac8a26c97e693527fefbd/a7bddd315c7740a7d065af9e80b0447fcd5b91be) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled module. The extract SHA-256 is `63256453dc9620091ce6af95f8b243b7da8f2ce76a585f4886f76aa6625b5b21`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. The extract is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior. The separately published 3.20 source catalog has a module file but none of these selected NIDs.

| Name | NID |
| --- | --- |
| `sceUpsrvDisableAutoInstall` | `-zy2KlVydQ8` |
| `sceUpsrvEndUpdateSession` | `6en3NM0YeJM` |
| `sceUpsrvNotifyUpdatedEvent` | `DKztVI6QmEU` |
| `sceUpsrvUpdateCheckDoCheckSystemWithDownloadTypeForSettings` | `yMBuJ0+PATE` |
| `sceUpsrvUpdateCheckDoCheckSystemWithReleaseType` | `-YtT5FR8mHg` |
| `sceUpsrvUpdateCheckIsTestPeriod` | `71dId93IAPw` |
| `sceUpsrvUpdateConfirmUpdate` | `Hyt+jnG8ktg` |
| `sceUpsrvUpdateStartUpdateModeWithParam` | `x72adjkymuQ` |
| `sceUpsrvUpdateStartUpdateMode` | `c2VqN+tPVMU` |
| `Unknown` | `33Z2z4kXKq0` |
| `Unknown` | `ItlDmxWppec` |
| `Unknown` | `SpnzKCdKA+4` |
| `Unknown` | `sEcvo34+ihg` |
| `Unknown` | `vrw9HD-1Gs0` |
| `Unknown` | `yyy3cEojLOw` |
