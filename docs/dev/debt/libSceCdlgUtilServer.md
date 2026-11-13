# libSceCdlgUtilServer technical debt

### Unknown function info

- [_ZN3sce16CommonDialogUtil6ServerC2EPKcmmis](../../../core/libs/prx/libSceCdlgUtilServer/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCdlgUtilServer.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce16CommonDialogUtil6ServerD0Ev, _ZN3sce16CommonDialogUtil6ServerD1Ev, _ZN3sce16CommonDialogUtil6ServerD2Ev](../../../core/libs/prx/libSceCdlgUtilServer/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCdlgUtilServer.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [`aFrwE9W9faU` and `W4zcizD4i3I`](../../../core/libs/prx/libSceCdlgUtilServer/Export.cpp) (libSceCdlgUtilServer) - names and signatures are unknown. Read from disassembly of firmware 13.42, not measured on a console: `aFrwE9W9faU` returns 0, as its body (`xor %eax,%eax; ret` at `0xf40`) does, and its no-parameter `int` declaration claims only the 32-bit return; `W4zcizD4i3I` returns the byte at offset 0x10 of the object its first argument points to, zero-extended (`movzbl 0x10(%rdi),%eax; ret` at `0x10e0`), and nothing else is read. What that byte means is unknown. A null object, which the module reads through, throws.
- [Server implementation object](../../../core/libs/prx/libSceCdlgUtilServer/Export.cpp) (libSceCdlgUtilServer) - not modelled: `setFinishData`, `setRun`, `setFinish`, `setResult`, `getVersion`, `isCloseReq`, `getServerState` and the ten unnamed exports below throw when the 64-bit pointer at offset 0x8 of their object is not null, where firmware 13.42 calls into it. With a null pointer each returns the module's fixed result, read from disassembly, not measured on a console: `0x80B8000E` for the four setters, `getVersion` (which writes nothing), `j6vGxXgVnCQ`, `ZRy0WQp2COs`, `aztojjv0xT0` and `VrN0lR+BJyc`; 0 for `isCloseReq`, `lhSjnuF2bzg` and `dQte4n-nQlg`; -1 for `getServerState`, `1IzIqsh2lVE`, `NKhtpQTUjpY` and `vfUweaM01gw`; the 16-bit `0xFFFF` for `qFygU3yA0HE`. A null object, which the module reads through, throws.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceCdlgUtilServer/Export.cpp) - 20 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unknown names use literal NID aliases. One data export is outside this callable inventory and requires separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `2pswOa0ZQoQ` |
| `Unknown` | `33zrWSbHxcI` |
| `Unknown` | `56KPLy5Uc-w` |
| `Unknown` | `CK-7W3zhCjk` |
| `Unknown` | `Dv1KWHTwGeM` |
| `Unknown` | `KaTogANOP6U` |
| `Unknown` | `MQDTRLwdnrU` |
| `Unknown` | `Pl1B1+pzwLM` |
| `Unknown` | `RiJYtG8H1Yk` |
| `Unknown` | `VMUTkgazl40` |
| `Unknown` | `WlSxsADNweU` |
| `Unknown` | `YNi5ipH59bE` |
| `Unknown` | `aX1ZGnOo9vg` |
| `Unknown` | `e3l9WkmIjyw` |
| `Unknown` | `ht7qLD4xIAU` |
| `Unknown` | `k9wR+1LoaoU` |
| `Unknown` | `mCWvwveN2gM` |
| `Unknown` | `ucY3L5rXqjs` |
| `Unknown` | `yPspG9QpJK4` |
| `Unknown` | `zjj3moy7KpM` |
