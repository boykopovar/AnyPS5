# libCss technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libCss/Export.cpp) - 65 entries are import-resolution declarations only. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `+zpMhCilHTQ` |
| `Unknown` | `-WR4-J79nn0` |
| `Unknown` | `45oyUIl85dg` |
| `Unknown` | `4qq5OHYaFU8` |
| `Unknown` | `6BDNwI1e59c` |
| `Unknown` | `6QhXfVttAyU` |
| `Unknown` | `74insBAQhg8` |
| `Unknown` | `7YdC9p2cTSQ` |
| `Unknown` | `81obgoH2X2g` |
| `Unknown` | `9T-RYQ7HwqI` |
| `Unknown` | `9wgr5dQqxyg` |
| `Unknown` | `AfC4Omb8svg` |
| `Unknown` | `B314wB+fU6g` |
| `Unknown` | `BpqeQbNU0qo` |
| `Unknown` | `CnQiuTNrJcM` |
| `Unknown` | `E96QCyLgJ3o` |
| `dvdAuthInitialize` | `Ej2QCEJXBec` |
| `DvdMac` | `EvvQwl+bIv4` |
| `Unknown` | `FTFSIzVj7gE` |
| `Unknown` | `FsJO2R6pcFY` |
| `Unknown` | `HaAxhqb2xoM` |
| `Unknown` | `Hm2XEUlTjSk` |
| `Unknown` | `IcknOdMyfxQ` |
| `Unknown` | `J6BYqrHA5UM` |
| `Unknown` | `KUvq4G5VBPo` |
| `Unknown` | `LAcpQCmLWwI` |
| `Unknown` | `MRbX1Lg7JPo` |
| `Unknown` | `OM3ptXltjzo` |
| `Unknown` | `Q4g0Ji0W5GY` |
| `Unknown` | `SRPTG7u8o98` |
| `Unknown` | `TJdtvWbNKhs` |
| `Unknown` | `UCT-EFL9GlY` |
| `Unknown` | `UlBK7McbIBI` |
| `Unknown` | `VLaRWTMO9U8` |
| `Unknown` | `VMBVlZevTLw` |
| `Unknown` | `Wdf6DHvrt4g` |
| `Unknown` | `X9dl5pCTGiE` |
| `Unknown` | `XWqQyI3K47s` |
| `dvdAuthFinalize` | `ZzaGM24XNFI` |
| `Unknown` | `bQpZTilHSL0` |
| `Unknown` | `cN12O1uQ6Fo` |
| `Unknown` | `cTnMCeNpGqc` |
| `Unknown` | `czKT3U30FFE` |
| `Unknown` | `daklpMlbo00` |
| `Unknown` | `f7iq9LsamYo` |
| `Unknown` | `gbja3RUMWJ4` |
| `Unknown` | `giIpEkyo9Hg` |
| `Unknown` | `hD3HhdNG+xw` |
| `Unknown` | `hbTTMmwE3lo` |
| `Unknown` | `hbvafh9uQqY` |
| `Unknown` | `jYGAlLsijgw` |
| `Unknown` | `k1+Z1AtnLYI` |
| `Unknown` | `k5AW5Xs4aqg` |
| `Unknown` | `kc5B1ELiUv4` |
| `Unknown` | `nhK3hP3XL3Y` |
| `Unknown` | `ohRg1gu5G1w` |
| `Unknown` | `pTycImStaM4` |
| `Unknown` | `rylZqUytLj8` |
| `Unknown` | `szOnkd+7b3U` |
| `Unknown` | `utwkEaQy6D4` |
| `Unknown` | `wDe5E-Nfd7E` |
| `Unknown` | `xQX0HWKQKDA` |
| `Unknown` | `z4KQSeB3+6U` |
| `CpsInitKey` | `z7D3a8pXEeE` |
| `Unknown` | `zxv6uaL0lAs` |

### Unresolved data exports

These nine older data exports remain undeclared. Their NIDs and sizes also appear in the later inventory. Three contain relocation entries in the older image, and the other six occupy non-file-backed storage. Their types, initialization and ownership remain unverified; no object contents or behavior are inferred from matching sizes.

| NID | Size (bytes) | Relocations |
| --- | ---: | ---: |
| `Ea79aJutCLc` | 40 | 3 |
| `+ItBPjmv86M` | 40 | 3 |
| `r6olfOgegxU` | 32 | 2 |
| `0wlN5i59xrM` | 1 | 0 |
| `dqpZ10KTssY` | 10 | 0 |
| `CjPeOAnJSIs` | 10 | 0 |
| `KZbHx6nNRQw` | 5 | 0 |
| `ZDKe0O0ynIs` | 5 | 0 |
| `mkFxjg4B3Dg` | 1 | 0 |
