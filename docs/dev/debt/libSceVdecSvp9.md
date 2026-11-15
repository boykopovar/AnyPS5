# libSceVdecSvp9 technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceVdecSvp9/Export.cpp) - 11 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `2dO21ExX5hM` |
| `Unknown` | `33wXf5Ne2xc` |
| `Unknown` | `BFFONbbPMXw` |
| `Unknown` | `JawAvxR7M+k` |
| `Unknown` | `RuWQMaRBlEs` |
| `Unknown` | `VGbdvyOsLzg` |
| `Unknown` | `VmDqnxqvIFg` |
| `Unknown` | `gktyGB+7-go` |
| `Unknown` | `hWezRodOVBI` |
| `Unknown` | `kUyX8iOM0E4` |
| `Unknown` | `x+HO8XFm6io` |
