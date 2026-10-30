# libSceXml technical debt

### Known export names; ABI prototypes unverified

- The 150 `sce::Xml` exports in [Export.cpp](../../../core/libs/prx/libSceXml/Export.cpp) are present in the PS5 3.20 `libSceXml` export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Each is named after the mangled symbol the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for its NID, and the NID of that name matches. Their prototypes and behavior are not established; each declaration uses the existing no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`. The declarations do not claim an ABI or behavior.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceXml/Export.cpp) - 33 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceXml.c) and the [selected export metadata](https://gist.github.com/kyon-eth/d35bcd082a172a28ada2052647b0ff20/72a4f16fcb238e61dc8c6424dc26eee106e6c8ce) from two version-labelled images (SHA-256 `3d83607a2ed81110a37d677e5ba1936a01cb78c5847a2e9b582a4a981d313050`). The labels are not authenticated. Prototypes and behavior are unverified; the declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| `Unknown` | `1JAZNsOKua0` |
| `Unknown` | `5JyhV10GpQE` |
| `Unknown` | `7MsYv4D1Ang` |
| `Unknown` | `AI6mDpNxPYk` |
| `Unknown` | `GceM6WE2WS4` |
| `Unknown` | `HGZ0DTuCvuQ` |
| `Unknown` | `I3ootbOM-00` |
| `Unknown` | `KgzBbi3DUDA` |
| `Unknown` | `N-2qBJYv6lw` |
| `Unknown` | `NZMWDTSdPXM` |
| `Unknown` | `PXmLYh0zF14` |
| `Unknown` | `Q-ryXoj4ez0` |
| `Unknown` | `QOmIaoXFNig` |
| `Unknown` | `REcO7rLo0fA` |
| `Unknown` | `VdYoc9kGeyA` |
| `Unknown` | `WPyn5rEy2DE` |
| `Unknown` | `XlzFT8nnAMU` |
| `Unknown` | `Zmk8Mb9qNQ0` |
| `Unknown` | `aZH+29bSsEk` |
| `Unknown` | `b9b6PA5TKc0` |
| `Unknown` | `fw4+tIZ-4t8` |
| `Unknown` | `jMWTTO6X-kw` |
| `Unknown` | `jc7L3HSltT0` |
| `Unknown` | `mODyaCpyDnM` |
| `Unknown` | `miVYGbk3LHs` |
| `Unknown` | `muE9OA3xYv4` |
| `Unknown` | `nfoWUQ1eDl8` |
| `Unknown` | `p0IeMemvZKo` |
| `Unknown` | `tZioKsu5pxs` |
| `Unknown` | `uiPVQSxXlsc` |
| `Unknown` | `ulLP+f5h0rg` |
| `Unknown` | `vWA1r9CiPgI` |
| `Unknown` | `wlgM8cLl1mQ` |
