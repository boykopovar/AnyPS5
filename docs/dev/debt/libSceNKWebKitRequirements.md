# libSceNKWebKitRequirements technical debt

### Known export names; ABI prototypes unverified

- 133 pending exports in [Export.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/Export.cpp) are present in the [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs) inventory. Their prototypes and behavior are unverified. They use the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNKWebKitRequirements/Export.cpp) - 32 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. These NIDs are absent from the pinned public 3.20 catalogue. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `++QInfMOBN0` |
| `Unknown` | `+oA7tlz2Aiw` |
| `Unknown` | `1MRhOnElUw4` |
| `Unknown` | `1T7K-PY5paE` |
| `Unknown` | `3yG30EKj-64` |
| `Unknown` | `4dTqu8uWd6Q` |
| `Unknown` | `6-rd4IXD0mM` |
| `Unknown` | `68aoiQnJutE` |
| `Unknown` | `8u1QXzSpyno` |
| `Unknown` | `JwB900c8Nyo` |
| `Unknown` | `KCP1Db-mniM` |
| `Unknown` | `NCGIxzXT-KA` |
| `Unknown` | `TAGTeghnmJs` |
| `Unknown` | `TNO-EUJZf3c` |
| `Unknown` | `Wz6vEl9ATDQ` |
| `Unknown` | `YpZ1fAOlVoo` |
| `Unknown` | `ZSkWe-fNsuY` |
| `Unknown` | `cgNin5rMqA8` |
| `Unknown` | `cq16C8ickjY` |
| `Unknown` | `hjlKZNuv+v8` |
| `Unknown` | `o6hmqtmcw10` |
| `Unknown` | `o7ktMsqiCBU` |
| `Unknown` | `rW6yS0aJbjQ` |
| `Unknown` | `rgWcud6b43M` |
| `Unknown` | `soW4BQ5l3Os` |
| `Unknown` | `tc5h+wVQWZI` |
| `Unknown` | `tu7+XEdsUA8` |
| `Unknown` | `u4SbDPju5eo` |
| `Unknown` | `uXf4oddEz5E` |
| `Unknown` | `uiTgPO1VRaA` |
| `Unknown` | `x2xMh95HEb4` |
| `Unknown` | `z5L7zirdMWM` |
