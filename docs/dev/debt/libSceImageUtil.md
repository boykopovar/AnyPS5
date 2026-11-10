# libSceImageUtil technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceImageUtil/Export.cpp) - 5 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `CttPwxlzc1A` |
| `Unknown` | `o9hdefxVx-0` |
| `Unknown` | `pjWv++MzaJY` |
| `Unknown` | `rSoaNeazErw` |
| `Unknown` | `yjU-5r3TDtc` |

- [Older callable NIDs](../../../core/libs/prx/libSceImageUtil/Export.cpp): 5 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.
