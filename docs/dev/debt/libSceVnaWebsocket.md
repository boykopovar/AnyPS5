# libSceVnaWebsocket technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceVnaWebsocket/Export.cpp) - 22 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unknown names use literal NID aliases. One data export is outside this callable inventory and requires separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `-IDFERYOJPs` |
| `Unknown` | `8KzE01OyRog` |
| `Unknown` | `94G0OCEujjA` |
| `Unknown` | `Bw0-YBkPNm4` |
| `Unknown` | `DP4477brt60` |
| `Unknown` | `J3fL2giuM+0` |
| `Unknown` | `J8-VXNQySz0` |
| `Unknown` | `JiBSw3cBh2g` |
| `Unknown` | `K-logS9LtFw` |
| `Unknown` | `KGPZ+BZgXn0` |
| `Unknown` | `MI6xIf-bHXs` |
| `Unknown` | `NS+C+w391Aw` |
| `Unknown` | `OUogBgTPp6E` |
| `Unknown` | `PPt1uQ9zOb8` |
| `Unknown` | `T4s2VWpD9-g` |
| `Unknown` | `es11O5Z1er4` |
| `Unknown` | `fxgYzL5aGyw` |
| `Unknown` | `hSnmIgcBCMk` |
| `Unknown` | `jtTGYZXUOqE` |
| `Unknown` | `obO4IYwAPtg` |
| `Unknown` | `qUkQKyPORE8` |
| `Unknown` | `t534VXlYZI8` |
