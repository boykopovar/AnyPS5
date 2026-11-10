# libSceAppDbShellCoreClient technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAppDbShellCoreClient/Export.cpp) - 10 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `-NB6hsKeI2E` |
| `Unknown` | `0PevQGrqaXQ` |
| `Unknown` | `1rdpvtPYXUg` |
| `Unknown` | `3GBO83N9XYk` |
| `Unknown` | `8ahcDF2NTFc` |
| `Unknown` | `DdycECIaV2o` |
| `Unknown` | `SlsrwedLC34` |
| `Unknown` | `aqObO3I7PVk` |
| `Unknown` | `gyhDBiNNvlg` |
| `Unknown` | `s3XFdWNwYD8` |

- [Older callable NIDs](../../../core/libs/prx/libSceAppDbShellCoreClient/Export.cpp): 18 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.
