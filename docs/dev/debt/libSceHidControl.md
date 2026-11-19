# libSceHidControl technical debt

### Functional

- [sceHidControlPswUpdateIsUpdateNeeded](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - returning `0x803b3001` (`ErrorInvalidArgument`) when the `result` pointer is unwritable via `GuestCopyOut_nid_no_patch` is a host-safety fallback preserving host errno; console behavior on unmapped pointers is not measured.

- [libSceHidControl](../../../core/libs/prx/libSceHidControl/Export.cpp) has no `/dev/hid` device behind it: AnyPS5's kernel models no HID control device, so `sceHidControlInit` gets the failed `open` and returns its -1, and every other export then returns `0x803B3002`. A request that does reach `ioctl`, for example on a regular file named `dev/hid` under the working directory, ends in the kernel's unsupported-request failure. What the group `'H'` (0x48) commands do and what their replies hold is not known. The library side follows firmware 13.42 `libSceHidControl.sprx` (sha256 `9640d14551bc2b4c360bfb1e633b2bca9b7fcf6efd51c2c4dab814de1156c25f`), read from disassembly to every return, not measured on a console. Closes when: a `/dev/hid` model exists and is measured against a console.
- [Request padding and unread replies](../../../core/libs/prx/libSceHidControl/Export.cpp) - bytes the 13.42 library leaves unset in a request, such as the first four bytes of the `sceHidControlGetAuthInformation` request, are zero here. `sceHidControlPswUpdateIsReady`, `sceHidControlPswUpdateGetStatus` and `sceHidControlPswUpdateGetReconnectStatus` translate their reply even when `ioctl` fails; on the console that reads a stack slot nothing wrote, here it reads 0.
- [Guest output fallback](../../../core/libs/prx/libSceHidControl/Export.cpp) - `sceHidControlPswUpdateIsUpdateNeededForMultiDevice`, `sceHidControlPswUpdateIsReady`, `sceHidControlPswUpdateGetStatus` and `sceHidControlPswUpdateGetReconnectStatus` move their outputs through `GuestCopyOut_nid_no_patch` (and the multi-device version input through `GuestCopyIn_nid_no_patch`) and return `0x803b3001` (`ErrorInvalidArgument`) when a guest pointer is unreadable or unwritable, preserving host errno where no ioctl runs; console behavior on unmapped pointers is not measured.
- [Argument meanings](../../../core/libs/prx/libSceHidControl/include/Api.hpp) - argument registers, widths and the checks on them come from the 13.42 disassembly; what each value means (device, port, handle, preset numbering) is not established, so the parameter names are neutral.

### Unknown function info

- [sceHidControlSetRevokeList](../../../core/libs/prx/libSceHidControl/Export.cpp) - not exported by firmware 13.42's `libSceHidControl.sprx`, so its 3.20 declaration still throws through `NotImplemented_nid_no_patch`.

### Additional callable export inventory

- [Additional exports](../../../core/libs/prx/libSceHidControl/Export.cpp) - 14 of these 19 NIDs have no name. The five named ones carry public candidate names that recompute to their NIDs; that settles the spelling, not that the console uses the names.

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
