# libSceAudioSystem technical debt

### Unknown function info

- [sceAudioSystemInit, sceAudioSystemProcessAdd, sceAudioSystemProcessRemove, sceAudioSystemProcessResume, sceAudioSystemProcessSuspend, sceAudioSystemTerminate](../../../core/libs/prx/libSceAudioSystem/Export.cpp) have unknown signatures and behaviour. The parameterless `int` declarations are import-resolution placeholders, not confirmed ABI signatures, and throw through `NotImplemented_nid_no_patch`. Only the names and NIDs are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceaudiosystem-bfe74a18cc.jsonl).
- [sceAudioSystemSystemResume and sceAudioSystemSystemSuspend](../../../core/libs/prx/libSceAudioSystem/Export.cpp) (libSceAudioSystem) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAudioSystem.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAudioSystem/Export.cpp) - 30 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/3fbb1a4a6f95a936b4cd58e9c3d8c50c/5e3af709224ed8235af11337309af0725e5685cb) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled image. The extract SHA-256 is `02ab27f12bcd0cc9fa03fa33f55fc201110f0d54a86bbbaa093d55d0a8cf262d`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. This is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior.

| Name | NID |
| --- | --- |
| `sceAudioOutServerBusGetIoParam` | `jhrJaWSScXs` |
| `sceAudioOutServerGetOption` | `D95bo7YrrGM` |
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
