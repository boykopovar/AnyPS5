# libSceSaveData technical debt

### Functional

- [libSceSaveData](../../../core/libs/prx/libSceSaveData/CMakeLists.txt) is built from the libSceSaveData.native sources, so a title that imports save data from `libSceSaveData`, as PS4 titles do, gets the same functions. The separate implementation it had before kept save-data memory in `_sd/sce_sdmemory/<user>/memory.dat` and mount aliases in `_sd/_aliases`; neither is read, so memory saved through that library is not found

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceSaveData/InventoryExports.cpp) - 14 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `0TAVrO-LgPI` |
| `Unknown` | `35cI-Z0nz68` |
| `Unknown` | `AuTE0gFxZCI` |
| `Unknown` | `IE+HJN8+Jdg` |
| `Unknown` | `IGpkv-UOup0` |
| `Unknown` | `NxxouhCA12c` |
| `Unknown` | `QaI8L6NzmJU` |
| `Unknown` | `VM8qEhWh-A4` |
| `Unknown` | `apvJPjG+6-Q` |
| `Unknown` | `cbD93i6sy9E` |
| `Unknown` | `fIu-ckf3n-o` |
| `Unknown` | `g1roz3Ux+3I` |
| `Unknown` | `q8KxzjSpPVw` |
| `Unknown` | `yBM2vzabhEA` |
