# libSceGLSlimVSH technical debt

### Unknown function info

- The 22 known-name imports in [Export.cpp](../../../core/libs/prx/libSceGLSlimVSH/Export.cpp) are listed in the PS5 3.20 [export inventory](https://github.com/DNNDHH/PS5-3.20_Libs). Their exact prototypes and behavior are unverified; each uses a no-parameter throwing import-resolution placeholder.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceGLSlimVSH/Export.cpp) - 7 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceGLSlimVSH.c) and the [selected export metadata](https://gist.github.com/kyon-eth/e1b845e2be0047e065a7dbb2c2e92322/7a180a59bf97bf6ea42020d4eedf9a8d631b0fd3) from two version-labelled images (SHA-256 `5d1eb4913d2914be044c05a48c4287a3a73e526605f21ec37ce9acfb5b2170ad`). The labels are not authenticated. Prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| `sceSlimglCompositorSetInvisibleCanvasCommand` | `DN8v2xNOyjg` |
| `sceSlimglCompositorSetMemoryCommand` | `aswH9c+RN0E` |
| `sceSlimglCompositorSetPostEventCommand` | `WvuRaahcTfA` |
| `sceSlimglCompositorWaitEndOfRendering` | `lcaik1AxwEY` |
| `sceSlimglCompositorWaitPostEvent` | `HwBlC8TLKHQ` |
| `sceSlimglIPCEnd` | `Fa0atOcWPaI` |
| `sceSlimglIPCSetup` | `F4pQ5tsDo74` |

### Additional unidentified exports

- Seven [callable exports](../../../core/libs/prx/libSceGLSlimVSH/Export.cpp) are declared as throwing placeholders. Their names, signatures and behavior are unverified. The same module and library identities occur in two version-labelled images; those labels are not authenticated.

| Name | NID |
| --- | --- |
| `Unknown` | `9iW854bwvmc` |
| `Unknown` | `DVNKmWX1jOU` |
| `Unknown` | `JjyNMbSAgLk` |
| `Unknown` | `LLDLZ-mQK+Q` |
| `Unknown` | `Yf0LATiXPsg` |
| `Unknown` | `ZLvm5korFRA` |
| `Unknown` | `qSuVNKHENqs` |
