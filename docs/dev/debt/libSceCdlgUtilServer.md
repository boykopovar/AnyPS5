# libSceCdlgUtilServer technical debt

### Unknown function info

- [_ZN3sce16CommonDialogUtil6Server13setFinishDataEPvm, _ZN3sce16CommonDialogUtil6Server6setRunEPvm, _ZN3sce16CommonDialogUtil6Server9setFinishEv, _ZN3sce16CommonDialogUtil6Server9setResultEi, _ZN3sce16CommonDialogUtil6ServerC2EPKcmmis](../../../core/libs/prx/libSceCdlgUtilServer/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCdlgUtilServer.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce16CommonDialogUtil6ServerD0Ev, _ZN3sce16CommonDialogUtil6ServerD1Ev, _ZN3sce16CommonDialogUtil6ServerD2Ev, _ZNK3sce16CommonDialogUtil6Server10getVersionERj, _ZNK3sce16CommonDialogUtil6Server10isCloseReqEv](../../../core/libs/prx/libSceCdlgUtilServer/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCdlgUtilServer.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZNK3sce16CommonDialogUtil6Server14getServerStateEv](../../../core/libs/prx/libSceCdlgUtilServer/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCdlgUtilServer.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceCdlgUtilServer/Export.cpp) - 32 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unknown names use literal NID aliases. One data export is outside this callable inventory and requires separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `1IzIqsh2lVE` |
| `Unknown` | `2pswOa0ZQoQ` |
| `Unknown` | `33zrWSbHxcI` |
| `Unknown` | `56KPLy5Uc-w` |
| `Unknown` | `CK-7W3zhCjk` |
| `Unknown` | `Dv1KWHTwGeM` |
| `Unknown` | `KaTogANOP6U` |
| `Unknown` | `MQDTRLwdnrU` |
| `Unknown` | `NKhtpQTUjpY` |
| `Unknown` | `Pl1B1+pzwLM` |
| `Unknown` | `RiJYtG8H1Yk` |
| `Unknown` | `VMUTkgazl40` |
| `Unknown` | `VrN0lR+BJyc` |
| `Unknown` | `W4zcizD4i3I` |
| `Unknown` | `WlSxsADNweU` |
| `Unknown` | `YNi5ipH59bE` |
| `Unknown` | `ZRy0WQp2COs` |
| `Unknown` | `aFrwE9W9faU` |
| `Unknown` | `aX1ZGnOo9vg` |
| `Unknown` | `aztojjv0xT0` |
| `Unknown` | `dQte4n-nQlg` |
| `Unknown` | `e3l9WkmIjyw` |
| `Unknown` | `ht7qLD4xIAU` |
| `Unknown` | `j6vGxXgVnCQ` |
| `Unknown` | `k9wR+1LoaoU` |
| `Unknown` | `lhSjnuF2bzg` |
| `Unknown` | `mCWvwveN2gM` |
| `Unknown` | `qFygU3yA0HE` |
| `Unknown` | `ucY3L5rXqjs` |
| `Unknown` | `vfUweaM01gw` |
| `Unknown` | `yPspG9QpJK4` |
| `Unknown` | `zjj3moy7KpM` |
