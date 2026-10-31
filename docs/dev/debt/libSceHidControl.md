# libSceHidControl technical debt

### Implemented from static analysis

- [libSceHidControl](../../../core/libs/prx/libSceHidControl/Export.cpp) - all 82 export functions are implemented from Ghidra 12.2.2 static analysis of `libSceHidControl.sprx` in the PS5 3.20 export inventory. Each function follows the decompiled per-function pattern: an `Init` guard (return `0x803b3002` "not ready" when uninitialized) followed by a single command send (the `PfccT7qURYE_C_D` style helper) to a device descriptor, then a post-send state check that aborts on mismatch; the two EQ-band value getters/setters return success pre-init per the decompiled bodies. Command op-codes, argument sizes and the `0x803b3001`/`0x803b3002` result codes are taken from the decompiled bodies. The host has no real device backend yet, so command results are the modeled `0x803b3001`/`0x803b3002`; this is a static-analysis model, not a console measurement.

### Unknown function info

- [14 unresolved NIDs](../../../core/libs/prx/libSceHidControl/Export.cpp) - `libSceHidControlUnknown_..._nid_no_patch` (the 14 unresolved NIDs, plus the 5 named candidates) are import-resolution declarations. The 5 named ones are modeled from the decompiled command-send pattern; the 14 unresolved remain explicit NID aliases. The parameterless `int` return is a static-analysis result, not a console-measured ABI.

### Additional callable export inventory

- [Additional exports](../../../core/libs/prx/libSceHidControl/Export.cpp) - 19 import-resolution declarations (5 named candidates and 14 unresolved NIDs). All entries are modeled from the decompiled `libSceHidControl.sprx` bodies. Parameterless `int` declarations are a static-analysis result, not a console-measured ABI.

#### Membership and identity evidence

The inspected `PT_SCE_MODULEPARAM` record contains version words `0x13590001` at offset `0x10` and `0x13600007` at offset `0x14`. The 13.60 label refers to the latter SDK-formatted word, not independently authenticated retail firmware or an introduction date. The [reproducible static audit](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298) records the extraction/filtering procedure and the observed NID-set digest. Each row below was read as a defined `STT_FUNC` entry; RVA and size are symbol-table values, not runtime addresses or function bytes.

Unique function NIDs in the inspected module: **81**. SHA-256 of sorted unique NIDs (one ASCII NID plus LF per row): `287013aa78cc6ad0042a719d9dab918c23d24bd5165a99dd1591e5152de3927d`. All 19 proposed NIDs are members of that set.

Candidate names marked **aerolib** occur in the [repository-pinned table](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv). Names marked **public candidate** occur in the [public ps5rs identity database at commit `2304be5131221bae0dac24b9767cabffda33f747`](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Every named candidate was recomputed with the repository NID algorithm and matched to a function NID in the inspected table; no name was assigned by ordinal or adjacent address. Those databases support spelling, not firmware attribution, prototypes or behavior. Unresolved names remain explicit NID aliases.

| Candidate / unresolved | NID | Name evidence | ELF RVA | Symbol bytes |
| --- | --- | --- | --- | ---: |
| `sceHidControlGetControllerSerialNumberInformation` | `0Jn+BBzGKH4` | public candidate | `0x2520` | 165 |
| `sceHidControlGetPswNearbyDeviceInfo` | `XmwsppH3YDI` | public candidate | `0xe00` | 110 |
| `sceHidControlPswUpdateInit` | `05Zuhe4dnWA` | public candidate | `0x1fb0` | 94 |
| `sceHidControlPswUpdateTerm` | `H6ctF1YKmCU` | public candidate | `0x23a0` | 94 |
| `sceHidControlSetPswControllerCommunicationMethod` | `ieH2et01RXU` | public candidate | `0x25d0` | 108 |
| `Unknown` | `00qes5NswlQ` | unresolved | `0x1f00` | 166 |
| `Unknown` | `8fIm7UR4VDc` | unresolved | `0x17e0` | 106 |
| `Unknown` | `AjBZjowWaFI` | unresolved | `0x1a80` | 107 |
| `Unknown` | `C05jP9w4PCc` | unresolved | `0xa40` | 90 |
| `Unknown` | `DUGI7AXFtV0` | unresolved | `0x24b0` | 100 |
| `Unknown` | `HZJRMD3BeKk` | unresolved | `0x540` | 107 |
| `Unknown` | `OFyJjudLKlA` | unresolved | `0x1760` | 118 |
| `Unknown` | `TKax3WNHtEg` | unresolved | `0x2290` | 98 |
| `Unknown` | `WikCB91EU0w` | unresolved | `0xd90` | 110 |
| `Unknown` | `i5ALHbvjRi8` | unresolved | `0x15a0` | 103 |
| `Unknown` | `iVF5VrBHJzk` | unresolved | `0x900` | 101 |
| `Unknown` | `tcDE01gUoN0` | unresolved | `0x8a0` | 96 |
| `Unknown` | `w2MEBm2Tvm8` | unresolved | `0xb70` | 90 |
| `Unknown` | `yJJq5Arw8EI` | unresolved | `0xaa0` | 95 |
