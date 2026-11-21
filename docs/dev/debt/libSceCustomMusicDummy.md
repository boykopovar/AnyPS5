# libSceCustomMusicDummy technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libSceCustomMusicDummy/Export.cpp) - 8 entries are import-resolution declarations only. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `7FCbsom9uGo` |
| `sceCustomMusicDummyFileCheckReachability` | `NpvAtwSD4EM` |
| `sceCustomMusicDummyKernelMmap` | `YMratqD+-tY` |
| `Unknown` | `YiW-KsUScf0` |
| `Unknown` | `YsG+NP9e8Io` |
| `Unknown` | `fzFy2-On5Dc` |
| `Unknown` | `jJA0xjAjkj4` |
| `sceCustomMusicDummyFileOpen` | `lJhv62O8rO0` |
