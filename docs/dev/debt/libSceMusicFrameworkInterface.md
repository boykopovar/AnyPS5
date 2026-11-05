# libSceMusicFrameworkInterface technical debt

### Unknown function info

- [sceMusicFwGetCurrentTime, sceMusicFwGetLoop, sceMusicFwGetMuted, sceMusicFwGetNetworkState, sceMusicFwGetVolume, sceMusicFwIsEnded, sceMusicFwIsPlaying, sceMusicFwIsStopped, sceMusicFwRemoveSrc, sceMusicFwSendMessageAsynchronously, sceMusicFwSendMessageSynchronously, sceMusicFwSetCurrentTime, sceMusicFwSetLoop, sceMusicFwSetShuffle, sceMusicFwSetVolume, sceMusicFwSkipNext and sceMusicFwSkipPrevious](../../../core/libs/prx/libSceMusicFrameworkInterface/Export.cpp) (libSceMusicFrameworkInterface) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceMusicFrameworkInterface.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Additional callable export inventory

- [Additional exports](../../../core/libs/prx/libSceMusicFrameworkInterface/Export.cpp) - 20 import-resolution declarations (19 named candidates and 1 unresolved NIDs). All entries call `NotImplemented_nid_no_patch`. Parameterless `int` declarations do not establish parameter or return ABI; no runtime implementation or title unblock is claimed.

#### Membership and identity evidence

The inspected `PT_SCE_MODULEPARAM` record contains version words `0x13590001` at offset `0x10` and `0x13600007` at offset `0x14`. The 13.60 label refers to the latter SDK-formatted word, not independently authenticated retail firmware or an introduction date. The [reproducible static audit](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298) records the extraction/filtering procedure and the observed NID-set digest. Each row below was read as a defined `STT_FUNC` entry; RVA and size are symbol-table values, not runtime addresses or function bytes.

Unique function NIDs in the inspected module: **37**. SHA-256 of sorted unique NIDs (one ASCII NID plus LF per row): `a6eae50cd87e410d4178fdf0de51208ed4d5916cb519472976033c81f58bcc7f`. All 20 proposed NIDs are members of that set.

Candidate names marked **aerolib** occur in the [repository-pinned table](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv). Names marked **public candidate** occur in the [public ps5rs identity database at commit `2304be5131221bae0dac24b9767cabffda33f747`](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Every named candidate was recomputed with the repository NID algorithm and matched to a function NID in the inspected table; no name was assigned by ordinal or adjacent address. Those databases support spelling, not firmware attribution, prototypes or behavior. Unresolved names remain explicit NID aliases.

| Candidate / unresolved | NID | Name evidence | ELF RVA | Symbol bytes |
| --- | --- | --- | --- | ---: |
| `sceMusicFwAbortMessage` | `vmCGqN-OFaY` | aerolib | `0x3930` | 17 |
| `sceMusicFwAddSrc` | `fr04nQ3Dkpc` | aerolib | `0xa70` | 883 |
| `sceMusicFwFinalize` | `3Y8a-RRYbhk` | aerolib | `0xa00` | 98 |
| `sceMusicFwGetCapability` | `scDaf7LO-PE` | aerolib | `0x2a80` | 359 |
| `sceMusicFwGetDuration` | `ccOo+E83pGY` | aerolib | `0x2950` | 290 |
| `sceMusicFwGetShuffle` | `FiQU5-kdM94` | aerolib | `0x31e0` | 290 |
| `sceMusicFwGetVolume2` | `sj8BkAXqNb0` | public candidate | `0x1e60` | 36 |
| `sceMusicFwInitialize` | `11wRJfoIJxE` | aerolib | `0x760` | 166 |
| `sceMusicFwInitialize2` | `Yn7uLcMKP1g` | public candidate | `0x940` | 182 |
| `sceMusicFwIsPaused` | `t5LNf+DD9OY` | aerolib | `0x25c0` | 290 |
| `sceMusicFwIsSeeking` | `Sim10+qqYUw` | aerolib | `0x2490` | 290 |
| `sceMusicFwLoad` | `cNLzn-IWmrw` | aerolib | `0x1540` | 268 |
| `sceMusicFwPause` | `HknA0+ZloHk` | aerolib | `0x1760` | 268 |
| `sceMusicFwPlay` | `4J-YAdBNHCo` | aerolib | `0x1650` | 268 |
| `sceMusicFwRegisterNotifyMessage` | `tTQ+AFZiGS0` | aerolib | `0x3950` | 355 |
| `sceMusicFwSetMuted` | `-lMRBsJ20fM` | aerolib | `0x1e90` | 284 |
| `sceMusicFwSetVolume2` | `8ixuO68kLfM` | public candidate | `0x1e40` | 22 |
| `sceMusicFwStop` | `5YQ-72Mdsuo` | aerolib | `0x1870` | 268 |
| `sceMusicFwUnregisterNotifyMessage` | `uAMoi7RinCQ` | aerolib | `0x3ac0` | 17 |
| `Unknown` | `X9GN6gWabWM` | unresolved | `0x2d50` | 321 |
