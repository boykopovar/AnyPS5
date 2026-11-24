# libSceUpdateService

## change [Additional libSceUpdateService exports]

- [Additional libSceUpdateService exports](../../../core/libs/prx/libSceUpdateService/Export.cpp) - the name/NID pairs below are defined exports in the [firmware 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceupdateservice-b7f2a7b06c.jsonl). Their ABIs and behaviour are unknown. Parameterless `int` declarations are import-resolution placeholders, not established signatures; what each call returns is in the "Every export" entry.

## change [Every export]

- [Every export](../../../core/libs/prx/libSceUpdateService/Export.cpp) (libSceUpdateService) - nothing after `sceUpsrvInitialize` is modelled. `sceUpsrvInitialize`, `sceUpsrvUpdateCheckIsTestPeriod` (it reads its arguments before the state) and the three exports the 13.42 module does not define (`sceUpsrvGetCompletedTaskId`, `sceUpsrvUpdateCancelBDUpdate`, `sceUpsrvUpdateGetImageWritingProgress`) throw. The other 51 give the 13.42 module's answer while its client pointer is null, which on this host is always, since only `sceUpsrvInitialize` sets it: 49 return `0x80182402` whatever their arguments, and `sceUpsrvTerminate` and `SpnzKCdKA+4` return 0. They must follow the client's state once `sceUpsrvInitialize` is implemented. Unknown: every prototype, the initialized behaviour and the name of `0x80182402`; nothing was measured on a console

## change [Firmware 13.42 module]

- [Firmware 13.42 module](../../../core/libs/prx/libSceUpdateService/Export.cpp) (libSceUpdateService) - only the paths taken before `sceUpsrvInitialize` were read. In `system/common/lib/libSceUpdateService.sprx` (sha256 `b2ec8840ae5fa326abd1035090b86fc3e784a0bf0ec740225aa1cc12266beeb8`) the client pointer `0x10018` lies in zero-fill memory; the module writes it only through `sceUpsrvInitialize` (`lea` at `0x2881`) and `sceUpsrvTerminate` (cleared at `0x2954`). The 51 exports were run under a CPU emulator with the pointer null, once with every argument register 0 and once with junk values: same result both times, no import reached. Each gated export compares `0x10018` with 0 (`cmpq $0x0`) and returns `0x80182402` when it is null (for example `sceUpsrvGetRebootFlag`, `0x4d90`-`0x4dd4`)

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceUpdateService/Export.cpp) - 15 entries observed in a 3.20-labelled image and a later 13.60-labelled image are import-resolution declarations only. Neither image label authenticates a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. What each entry returns is in the "Every export" entry. Unidentified names use literal NID aliases. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).
