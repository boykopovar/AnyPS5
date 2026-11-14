# libSceResourceArbitrator technical debt

### Additional callable export inventory

- [Additional exports](../../../core/libs/prx/libSceResourceArbitrator/Export.cpp) - 14 import-resolution declarations (7 named candidates and 7 unresolved NIDs). All entries call `NotImplemented_nid_no_patch`. Parameterless `int` declarations do not establish parameter or return ABI; no runtime implementation or title unblock is claimed.

#### Membership and identity evidence

The inspected `PT_SCE_MODULEPARAM` record contains version words `0x13590001` at offset `0x10` and `0x13600007` at offset `0x14`. The 13.60 label refers to the latter SDK-formatted word, not independently authenticated retail firmware or an introduction date. The [reproducible static audit](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298) records the extraction/filtering procedure and the observed NID-set digest. Each row below was read as a defined `STT_FUNC` entry; RVA and size are symbol-table values, not runtime addresses or function bytes.

Unique function NIDs in the inspected module: **14**. SHA-256 of sorted unique NIDs (one ASCII NID plus LF per row): `fd766716d98fc4ed8ecea0d6b4cb81602405a5deadc62fc98efeb3a1850cd179`. All 14 proposed NIDs are members of that set.

Candidate names marked **aerolib** occur in the [repository-pinned table](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv). Names marked **public candidate** occur in the [public ps5rs identity database at commit `2304be5131221bae0dac24b9767cabffda33f747`](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Every named candidate was recomputed with the repository NID algorithm and matched to a function NID in the inspected table; no name was assigned by ordinal or adjacent address. Those databases support spelling, not firmware attribution, prototypes or behavior. Unresolved names remain explicit NID aliases.

| Candidate / unresolved | NID | Name evidence | ELF RVA | Symbol bytes |
| --- | --- | --- | --- | ---: |
| `sceResourceArbitratorChangeState` | `CYjfezkWcaE` | aerolib | `0x10b0` | 168 |
| `sceResourceArbitratorClearCallback` | `LqmYZGB-UEQ` | aerolib | `0xee0` | 172 |
| `sceResourceArbitratorGetFeatureIdList` | `fBX+DBOwNis` | public candidate | `0x11f0` | 308 |
| `sceResourceArbitratorIsExecutable` | `3iZXcGeRgJk` | aerolib | `0x1160` | 142 |
| `sceResourceArbitratorPostTelemetry` | `GmaE0Ew4h4s` | aerolib | `0x1420` | 168 |
| `sceResourceArbitratorSendDebugFunction` | `mtLIcm9hwFM` | aerolib | `0x1b30` | 108 |
| `sceResourceArbitratorSetCallback` | `kJvjeet+f9I` | aerolib | `0xdb0` | 292 |
| `Unknown` | `3xRoVp7fPPY` | unresolved | `0x1a90` | 157 |
| `Unknown` | `OM6SXhDZdiU` | unresolved | `0x1550` | 229 |
| `Unknown` | `TYiIrKTzQ8s` | unresolved | `0x17a0` | 157 |
| `Unknown` | `cT1vA0DTMcs` | unresolved | `0x14d0` | 120 |
| `Unknown` | `jzKwHzhEBIQ` | unresolved | `0x1330` | 235 |
| `Unknown` | `rMJnXXj6GIo` | unresolved | `0x1840` | 396 |
| `Unknown` | `ru9qkt9UCLQ` | unresolved | `0x1640` | 348 |
