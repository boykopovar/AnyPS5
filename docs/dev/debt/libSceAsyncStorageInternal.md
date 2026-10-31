# libSceAsyncStorageInternal technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAsyncStorageInternal/Export.cpp) - 13 entries present in both version-labelled images are import-resolution declarations only. A public 3.20 catalogue for this library was not verified. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `+YiZLBbdEy8` |
| `Unknown` | `65H7-ohmD9c` |
| `Unknown` | `93CwGWrhf+w` |
| `Unknown` | `9zqbh2tZtZg` |
| `Unknown` | `EKjWxMVMPzY` |
| `Unknown` | `Knldkc0w9lI` |
| `Unknown` | `TN9xRCVggRw` |
| `Unknown` | `hP+HenjaOi8` |
| `Unknown` | `iYDbqYNVajM` |
| `Unknown` | `jR1uFCw8qwc` |
| `Unknown` | `mvZ-UdW+Z-Q` |
| `Unknown` | `q4KNqhT5ibk` |
| `Unknown` | `r299OxwCYsY` |

- [Older callable NIDs](../../../core/libs/prx/libSceAsyncStorageInternal/Export.cpp): 9 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory. Five use the primary library qualifier and four use the auxiliary qualifier; qualified guest import resolution is unverified.
