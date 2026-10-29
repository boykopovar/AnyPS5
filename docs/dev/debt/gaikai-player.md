# gaikai-player technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/gaikai-player/Export.cpp) - 98 entries are import-resolution declarations only. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `+BOHW2L5FV4` |
| `Unknown` | `-HFHgLUYu44` |
| `Unknown` | `-xjpiP4Nbj4` |
| `Unknown` | `07Fs53QzHAg` |
| `Unknown` | `3147pBrIgG8` |
| `Unknown` | `3z7jV3EMJmY` |
| `Unknown` | `4UWulkc5q6M` |
| `Unknown` | `4W-RRo5UOR8` |
| `Unknown` | `4ovsw+Vql3c` |
| `_ZN2GK9PlayerPSN13initializeLibEb` | `4zord7MtPtw` |
| `Unknown` | `5EIiJZVCPQs` |
| `Unknown` | `5U+eWXoEDqg` |
| `Unknown` | `7J+aUwe+1Uw` |
| `Unknown` | `7Xy4kOd2xls` |
| `Unknown` | `8ScNIr3r+LA` |
| `Unknown` | `91AcXU+irqw` |
| `Unknown` | `ABnxAq6yWPk` |
| `Unknown` | `AuWi7iSC-q0` |
| `Unknown` | `BzILFX1oZ-0` |
| `Unknown` | `CTq514omwSA` |
| `Unknown` | `E0sUTMlJn8g` |
| `Unknown` | `ECxqgN1rAWQ` |
| `Unknown` | `EHIvcajktdE` |
| `_ZN2GK9PlayerPSNaSERKS0_` | `EHxdFvoxzCM` |
| `Unknown` | `EbGvpY6emd0` |
| `_ZN2GK9PlayerPSNC2ERKS0_` | `EmrfDsAor7Q` |
| `_ZN2GK9PlayerPSN17cancelRequestGameEv` | `EzAwc3tWi+o` |
| `Unknown` | `FH6A3+uPeiw` |
| `_ZN2GK9PlayerPSND2Ev` | `FmqwUp-Kius` |
| `Unknown` | `GSn8xl0FiKM` |
| `Unknown` | `GbdjbevMA-c` |
| `Unknown` | `GqT3Hql+eQc` |
| `Unknown` | `HBDiBzlPrLo` |
| `Unknown` | `HsswMIOn2-A` |
| `Unknown` | `IH2B9LWzKaM` |
| `_ZN2GK9PlayerPSN4stopEv` | `J1Tu68RHMaA` |
| `Unknown` | `L2PcDPQF0Z0` |
| `Unknown` | `LlOCEUwKjW0` |
| `_ZN2GK9PlayerPSN15requestClientIdEv` | `LpxdSc7EQM4` |
| `Unknown` | `MCREIc6YSoI` |
| `Unknown` | `MFSp0u6sdT0` |
| `Unknown` | `NMKEM3Ler6w` |
| `Unknown` | `NWtOvX8xYrk` |
| `Unknown` | `NYsLJkkgu-A` |
| `Unknown` | `Nw3wlTYQO+g` |
| `Unknown` | `Ra3+zn9t1EE` |
| `Unknown` | `T0Z423-4dM4` |
| `Unknown` | `TDvGoX4m+iU` |
| `Unknown` | `TGclDG-aT-A` |
| `Unknown` | `TH5sTdU2EYo` |
| `Unknown` | `TqeRJnMMIj4` |
| `Unknown` | `VTK5rhMMXd8` |
| `Unknown` | `VwmkqbCDt10` |
| `Unknown` | `WNJTO-AUSNk` |
| `Unknown` | `XcFlokW+11c` |
| `_ZN2GK9PlayerPSND0Ev` | `Y7B0vc8885c` |
| `Unknown` | `Yp84vGq8C4g` |
| `_ZN2GK9PlayerPSN11setSettingsEPKcb` | `ZME71iqnOSo` |
| `Unknown` | `Ze8vWTqoB1w` |
| `Unknown` | `aS3eoSgpX5E` |
| `Unknown` | `asM9Q-FxPls` |
| `Unknown` | `b51UgF+G6hI` |
| `Unknown` | `bgiwmJN3ab4` |
| `Unknown` | `cmIYlr4WnhA` |
| `Unknown` | `dHNruFnl0GQ` |
| `_ZN2GK9PlayerPSNC1ERKS0_` | `dZUSSMQ9SwI` |
| `Unknown` | `eLEc6KZ3kqI` |
| `Unknown` | `fji58uglItQ` |
| `Unknown` | `hoLrV2BDmHY` |
| `_ZN2GK9PlayerPSND1Ev` | `iDTIi7G3YxU` |
| `Unknown` | `iVWPgza6CQM` |
| `Unknown` | `io15NBrfbak` |
| `Unknown` | `j39hl-fK+RE` |
| `Unknown` | `jyuXmr1+ov0` |
| `Unknown` | `l+PmVwgFZFc` |
| `Unknown` | `l1iOUKe7ui4` |
| `Unknown` | `mOXyQ30KxSY` |
| `Unknown` | `mQOCCCCr5qs` |
| `Unknown` | `oRSrMWryckQ` |
| `Unknown` | `oSrpHIwn9DM` |
| `Unknown` | `omj4fJ-2fjQ` |
| `Unknown` | `p5zs+sOxJiA` |
| `Unknown` | `pDx5VDFecRU` |
| `Unknown` | `pNbygJyUB2U` |
| `Unknown` | `q4Xyn-daMXM` |
| `_ZN2GK9PlayerPSN11requestGameEb` | `q9kzJ0gemWs` |
| `Unknown` | `rIroNwmw2Yg` |
| `Unknown` | `rrCp+msrByg` |
| `Unknown` | `rvD27gUwcr4` |
| `Unknown` | `tZEuGqXarjk` |
| `_ZN2GK9PlayerPSN15cancelStartGameEv` | `ue3o0cfflkM` |
| `Unknown` | `vVHCcfjPK5s` |
| `_ZN2GK9PlayerPSN11shutdownLibEv` | `vsOZMpVlVUo` |
| `Unknown` | `w+F64d08kLU` |
| `Unknown` | `weVOkh9oJ8E` |
| `Unknown` | `x5gAPzs-S3w` |
| `Unknown` | `xwA3ryFwmWQ` |
| `Unknown` | `yafbw+0vFx4` |

### Unresolved module identity and older exports

- [Provider identity](../../../core/libs/prx/gaikai-player/Export.cpp) - The image uses logical module identity `gaikai_player_lib`, which differs from its physical filename. Consumer-side dependency mapping has not been established, so loading and qualified imports remain unverified.

- [Data exports](../../../core/libs/prx/gaikai-player/Export.cpp) - Nine older data exports remain unverified. Three contain relocations; the remaining six occupy non-file-backed storage. Their layout, initialization and ownership are unknown.

The following 14 older callable NIDs are absent from the later module export table. None of their bodies exactly matched a later module export body. Changed implementations remain possible. Candidate names match their NIDs but do not establish signatures.

| Candidate name | NID |
| --- | --- |
| `_ZN2GK9PlayerPSN5resetEv` | `79UCYGe6xtQ` |
| `Unknown` | `8MLkuiMlU7s` |
| `Unknown` | `F8088eadVDU` |
| `Unknown` | `Gds14k0pS4k` |
| `Unknown` | `IUSzwnbSXrU` |
| `_ZN2GK9PlayerPSN14testConnectionEv` | `NOL3hrPcMVU` |
| `Unknown` | `O2FzUx-BLA0` |
| `Unknown` | `P4kESgIfY+U` |
| `Unknown` | `mC+9gdlEFwk` |
| `Unknown` | `nGLM7yskqdQ` |
| `Unknown` | `ticKs11rR2g` |
| `Unknown` | `uyWBXH+IM4w` |
| `Unknown` | `vxYyFYQ9LIs` |
| `Unknown` | `w27Vr6tB4qg` |
