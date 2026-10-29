# libSceAudioSystem technical debt

### Unknown function info

- [sceAudioSystemSystemResume and sceAudioSystemSystemSuspend](../../../core/libs/prx/libSceAudioSystem/Export.cpp) (libSceAudioSystem) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAudioSystem.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAudioSystem/Export.cpp) - 36 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `sceAudioOutServerBusGetIoParam` | `jhrJaWSScXs` |
| `sceAudioOutServerGetOption` | `D95bo7YrrGM` |
| `sceAudioSystemInit` | `7EaoqeyirPY` |
| `sceAudioSystemProcessAdd` | `egc9mLq5Pxg` |
| `sceAudioSystemProcessRemove` | `eacQKJ1yA2E` |
| `sceAudioSystemProcessResume` | `I+AmWGX+bqk` |
| `sceAudioSystemProcessSuspend` | `RC9uu4hI-w0` |
| `sceAudioSystemTerminate` | `OSvFwsC7tEo` |
| `Unknown` | `-e5aJEqL34Y` |
| `Unknown` | `17nOsxccH5o` |
| `Unknown` | `2bKJUeywpeY` |
| `Unknown` | `3HOelmA2vcw` |
| `Unknown` | `5SM0uQwcgTQ` |
| `Unknown` | `5oOYYvwIb0k` |
| `Unknown` | `6PGbkl6OnvE` |
| `Unknown` | `9QaKJdd7jKk` |
| `Unknown` | `B-JyTVr90kc` |
| `Unknown` | `Fd9u7-OKW+Q` |
| `Unknown` | `FkFcs7NTFDo` |
| `Unknown` | `GUtNdBeMrcM` |
| `Unknown` | `HxLyu3KUv5U` |
| `Unknown` | `IiO+DdVz-4E` |
| `Unknown` | `J2Fx9JLKcqU` |
| `Unknown` | `Met2nIMg+Ng` |
| `Unknown` | `WRDd2feDYzg` |
| `Unknown` | `c4m3OSO0lj8` |
| `Unknown` | `ds20PmJfSSk` |
| `Unknown` | `hjlhfF8Voz0` |
| `Unknown` | `jLjHQbmK9+A` |
| `Unknown` | `jZPcy9MG4w0` |
| `Unknown` | `lSatNsWSmps` |
| `Unknown` | `ltR0PMUM270` |
| `Unknown` | `mS9gpe47Eio` |
| `Unknown` | `tHodLaUv04w` |
| `Unknown` | `uoPBJTVWhrg` |
| `Unknown` | `xG7GUeOKtBM` |
