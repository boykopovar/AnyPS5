# libSceNpCommon technical debt

### Unknown function info

Pending import declarations are matched to names in the local libSceNpCommon.sprx export table and the public [PS5 3.20 libSceNpCommon export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/main/libSceNpCommon.c); their NIDs are recomputed with tools/nid_names.py. Prototypes and behavior are unknown. Each parameterless declaration exists for import resolution and throws if called.

- [sceNpCloseSema](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `8hPzfjZzV88`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpGetNavSdkVersion](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `QmDEFikd3VA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpIntGetPlatformType](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `G6qytFoBJ-w`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpIntSetPlatformType](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `X6emt+LbSEI`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpJsonParse](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `vJGDnNh4I0g`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpJsonParseBuf](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `RgfCYkjW7As`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpJsonParseBufInit](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `SnAdybtBK3o`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpJsonParseEx](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `p5ZkSMRR7AU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpJsonParseExInit](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `nhgjiwPUIzI`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpJsonParseInit](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `teVnFAL6GNY`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpOpenSema](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `-blITIdtUd0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpServerErrorJsonGetErrorCode](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `0lZHbA-HRD0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpServerErrorJsonMultiGetErrorCode](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `cRabutqUG7c`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpServerErrorJsonParse](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `WSQxnAVLKgw`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpServerErrorJsonParseInit](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `UbStlMKTBeU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpServerErrorJsonParseMultiInit](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `hbe+DdooIi4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpSetPlatformType](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `m9JzZSoDVFY`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilCanonicalizeNpIdForPs4](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `4mEAk-UKVNw`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilCanonicalizeNpIdForPsp2](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `N3FB4r8JoRE`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetDateSetAuto](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `owm52JoZ8uc`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetDbgCommerce](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `1Gfhi+tZ9IE`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetEnv](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `kBON3bAtfGs`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetFakeDisplayNameMode](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `MUj0IV6XFGs`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetFakeRateLimit](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `O86rgZ2azfg`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetIgnoreNpTitleId](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `FrxliFYAO8Y`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetNpDebug](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `GRvK1ZE+FEQ`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetNpTestPatch](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `OA8f3KF9JsM`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetShareTitleCheck](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `fB5hE65pzbU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetSystemLanguage](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `SXUNKr9Zkv0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetTrcNotify](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `AjzLvR0g5Zs`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.

### Additional callable exports

- The [revision-pinned identity inventory](https://gist.github.com/kyon-eth/c5c2b80f9df9b48496fbf773b2aa307a/7143affa622c9b33588346e8c08f3ff49b65e808#file-npcommon-callable-identities-768-json) records a 768-entry superset of the module, library, symbol type and NIDs for the declarations in [InventoryExports.cpp](../../../core/libs/prx/libSceNpCommon/InventoryExports.cpp); it is a metadata summary rather than independent verification. For the 415 declarations absent from the public catalogue, the 420-entry superset of [per-image ELF records](https://gist.github.com/kyon-eth/c5c2b80f9df9b48496fbf773b2aa307a/7143affa622c9b33588346e8c08f3ff49b65e808#file-npcommon-catalogue-absent-420-json) include the raw qualified names, module and library identities, symbol type, values and sizes; the record file SHA-256 is `516d3c70069d1cc3aeed837cf5907fdb6a5834c33b34d60e3f43e0d0356e6e7b`. Image labels are not authenticated.

- [Additional callable exports](../../../core/libs/prx/libSceNpCommon/InventoryExports.cpp) - 707 declarations throw through `NotImplemented_nid_no_patch`; signatures and behavior are unverified. The later 13.60-labelled image contains all 707; 292 match the public 3.20 catalogue, while 415 are absent and their presence in retail 3.20 is unverified. Per-image records show the latter in a separate 3.20-labelled image, but neither image label is authenticated. Identified names match NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Data exports need separate analysis.

| Name | NID |
| --- | --- |
| `_ZN3sce2np10CancelLock3EndEPKciS3_` | `nleHqndSeQ0` |
| `_ZN3sce2np10CancelLock5BeginEPNS0_6HandleEPKciS5_` | `lJ2Efd9PUKI` |
| `_ZN3sce2np10CancelLockC1Ev` | `Vq9LKkPXkIQ` |
| `_ZN3sce2np10CancelLockD1Ev` | `K7FjXiy2z+A` |
| `_ZN3sce2np10CancelLockD2Ev` | `1iHBAKrdE90` |
| `_ZN3sce2np10Cancelable10IsCanceledEv` | `LJvHO3uCNm4` |
| `_ZN3sce2np10Cancelable11CheckCancelEPKciS3_` | `IwDQAbQxvD0` |
| `_ZN3sce2np10Cancelable12UnlockCancelEPKciS3_` | `-zbpF68OGDs` |
| `_ZN3sce2np10Cancelable13SetCancelableEb` | `bBLapYYwyr0` |
| `_ZN3sce2np10Cancelable14SetupSubCancelEPS1_PKciS4_` | `j4gLOIpHgNk` |
| `_ZN3sce2np10Cancelable16CleanupSubCancelEPS1_` | `vmt3ZOlQu3o` |
| `_ZN3sce2np10Cancelable4InitEv` | `Y7f+qBjKxdo` |
| `_ZN3sce2np10Cancelable6CancelEij` | `Jhbrpz0YhHU` |
| `_ZN3sce2np10CancelableD0Ev` | `kdOC-2AE06w` |
| `_ZN3sce2np10CancelableD2Ev` | `vZXDqs2x7t0` |
| `_ZN3sce2np10EventQueue10ClearAbortEt` | `aoas3bJANfY` |
| `_ZN3sce2np10EventQueue10TryDequeueEPvm` | `QlP4t2SGZ4I` |
| `_ZN3sce2np10EventQueue4InitEPKcmm` | `b20e017Ei94` |
| `_ZN3sce2np10EventQueue4dtorEv` | `N1gnYosdK7Q` |
| `_ZN3sce2np10EventQueue5AbortEt` | `slmKkuIoC28` |
| `_ZN3sce2np10EventQueue7DequeueEPvmj` | `suxln7PooIo` |
| `_ZN3sce2np10EventQueue7DestroyEv` | `qvpEuKumIGM` |
| `_ZN3sce2np10EventQueueC1EP16SceNpAllocatorEx` | `OGAIG7dVmUk` |
| `_ZN3sce2np10EventQueueD1Ev` | `lQXgvDXBGtA` |
| `_ZN3sce2np10EventQueueD2Ev` | `8kUkQPQP7bA` |
| `_ZN3sce2np10JsonNumber5ClearEv` | `YHNEgBCSL2o` |
| `_ZN3sce2np10JsonNumber6SetNumEPKc` | `VyCn9EVJGlU` |
| `_ZN3sce2np10JsonNumber6SetNumEi` | `UgmqDr1BCLw` |
| `_ZN3sce2np10JsonNumber6SetNumEj` | `PccynQ5NdVQ` |
| `_ZN3sce2np10JsonNumber6SetNumEl` | `MY0CSk24EcY` |
| `_ZN3sce2np10JsonNumber6SetNumEm` | `qbW7qOvVafI` |
| `_ZN3sce2np10JsonObject12SetFieldBoolEPKcbPPNS0_8JsonBoolE` | `a0-dxlANjcs` |
| `_ZN3sce2np10JsonObject13GetFieldArrayEPKc` | `2kC579f2EYU` |
| `_ZN3sce2np10JsonObject13GetFieldArrayEiPPKc` | `AUQ8VIY73SA` |
| `_ZN3sce2np10JsonObject13SetFieldArrayEPKcPPNS0_9JsonArrayE` | `DfTMqdyp50I` |
| `_ZN3sce2np10JsonObject13SetFieldArrayEPKcRKNS0_9JsonArrayEPPS4_` | `20UR1EhRDsQ` |
| `_ZN3sce2np10JsonObject14GetFieldObjectEPKc` | `fSOp3EWdbRg` |
| `_ZN3sce2np10JsonObject14GetFieldObjectEiPPKc` | `9eR-lVD3oUc` |
| `_ZN3sce2np10JsonObject14SetFieldNumberEPKciPPNS0_10JsonNumberE` | `FWazWMq-JhI` |
| `_ZN3sce2np10JsonObject14SetFieldNumberEPKcjPPNS0_10JsonNumberE` | `rHXGiBNSNQU` |
| `_ZN3sce2np10JsonObject14SetFieldNumberEPKclPPNS0_10JsonNumberE` | `wphSXO9vsoM` |
| `_ZN3sce2np10JsonObject14SetFieldNumberEPKcmPPNS0_10JsonNumberE` | `5jmpfPn-FDA` |
| `_ZN3sce2np10JsonObject14SetFieldObjectEPKcPPS1_` | `gEcOVRHVygA` |
| `_ZN3sce2np10JsonObject14SetFieldObjectEPKcRKS1_PPS1_` | `T3jrb8S18h8` |
| `_ZN3sce2np10JsonObject14SetFieldStringEPKcS3_PPNS0_10JsonStringE` | `w8mFPV1NRdQ` |
| `_ZN3sce2np10JsonObject16DeleteFieldValueEPKc` | `-WgnISXjJ7A` |
| `_ZN3sce2np10JsonObject5ClearEv` | `DiHxx2k5zfM` |
| `_ZN3sce2np10JsonParser17JsonParseCallbackEP14JsonParseInfo2ijjPKcPK13JsonValueElm2iPv` | `yllzeo7Bu74` |
| `_ZN3sce2np10JsonParser17jsonParseCallbackEP14JsonParseInfo2ijjPKcPK13JsonValueElm2i` | `CPPgrzZk8nU` |
| `_ZN3sce2np10JsonParser4InitEPK7JsonDefPNS1_12EventHandlerE` | `AGadQiCfKDY` |
| `_ZN3sce2np10JsonParser5ParseEPKcm` | `CDzSgHA6hWg` |
| `_ZN3sce2np10JsonParser5ParseEPNS0_6HandleEPNS0_12StreamReaderEPNS0_9StreamCtxE` | `KnMt8zGsyzc` |
| `_ZN3sce2np10JsonParserC1EP16SceNpAllocatorEx` | `2fB55i3uWyk` |
| `_ZN3sce2np10JsonParserC2EP16SceNpAllocatorEx` | `ZJbPQt+FTnY` |
| `_ZN3sce2np10JsonParserD0Ev` | `u+A16O-TAHk` |
| `_ZN3sce2np10JsonParserD1Ev` | `qJb7IXDg9xk` |
| `_ZN3sce2np10JsonParserD2Ev` | `AvvE5A5A6ZA` |
| `_ZN3sce2np10JsonString5ClearEv` | `kXE1imLw7yo` |
| `_ZN3sce2np10JsonString6SetStrEPKc` | `SN4IgvT26To` |
| `_ZN3sce2np10MemoryFile11GetFileSizeEPl` | `uu5cOJCNYts` |
| `_ZN3sce2np10MemoryFile11WriteToFileEPNS0_6HandleEPNS0_4FileE` | `XVW7-UURDhY` |
| `_ZN3sce2np10MemoryFile12ExtendBufferEv` | `9uk3FNGpOc8` |
| `_ZN3sce2np10MemoryFile12ReadFromFileEPNS0_6HandleEPNS0_4FileE` | `O06P-AD8fqQ` |
| `_ZN3sce2np10MemoryFile4OpenEPKNS1_9OpenParamE` | `i176qqzgtGw` |
| `_ZN3sce2np10MemoryFile4ReadEPNS0_6HandleEPvmPm` | `EyhtbPFMWNA` |
| `_ZN3sce2np10MemoryFile4SeekEliPl` | `DnRJsdPZjAE` |
| `_ZN3sce2np10MemoryFile4SyncEv` | `AZTMWob-mog` |
| `_ZN3sce2np10MemoryFile5CloseEv` | `dl6+SFHLke0` |
| `_ZN3sce2np10MemoryFile5WriteEPNS0_6HandleEPKvmPm` | `r2O0f9X-mqs` |
| `_ZN3sce2np10MemoryFileC1EP16SceNpAllocatorEx` | `WY5g+GKxFB4` |
| `_ZN3sce2np10MemoryFileC2EP16SceNpAllocatorEx` | `ev77AviWYu8` |
| `_ZN3sce2np10MemoryFileD2Ev` | `lGjyfcI++PY` |
| `_ZN3sce2np10NpCommSign5ClearEv` | `t6mpRNvX4QA` |
| `_ZN3sce2np10NpCommSign7SetDataEPKhm` | `JOjsUdFJ+hU` |
| `_ZN3sce2np10NpCommSign8BuildHexERKS1_Pcmb` | `PNspCKzuOm8` |
| `_ZN3sce2np10NpCommSign8ParseHexEPS1_PKcm` | `5aRK9tfUiv0` |
| `_ZN3sce2np10NpCommSignC1ERK27SceNpCommunicationSignature` | `4lUwFkt-ZZ8` |
| `_ZN3sce2np10NpCommSignC1ERKS1_` | `5ZKpMgMCC7s` |
| `_ZN3sce2np10NpCommSignC1Ev` | `5GW51rYObX0` |
| `_ZN3sce2np10NpCommSignC2ERK27SceNpCommunicationSignature` | `AgpHmnT1+6w` |
| `_ZN3sce2np10NpCommSignC2ERKS1_` | `KFMVo5CoWpQ` |
| `_ZN3sce2np10NpCommSignC2Ev` | `51FZZoJ3XYM` |
| `_ZN3sce2np10NpCommSignD0Ev` | `rpYQprUheiM` |
| `_ZN3sce2np10NpCommSignD1Ev` | `DTWRMRckGvk` |
| `_ZN3sce2np10NpCommSignD2Ev` | `C7bDewPzXYk` |
| `_ZN3sce2np12HttpTemplate16SetSCEOnlyOptionEv` | `YgbTkTF1Iyg` |
| `_ZN3sce2np12HttpTemplate4InitEiPKcib` | `iOTsJTR6Y9U` |
| `_ZN3sce2np12HttpTemplateC1Ev` | `Vj7HiXK-tTg` |
| `_ZN3sce2np12HttpTemplateD0Ev` | `cXYOwTVAuMs` |
| `_ZN3sce2np12HttpTemplateD2Ev` | `h6XPsGpHAtc` |
| `_ZN3sce2np12SelectorItem4InitEPKc` | `1So3qQHgSyE` |
| `_ZN3sce2np12SelectorItem6NotifyEv` | `0yb1wmzIG44` |
| `_ZN3sce2np12SelectorItem9GetEventsEv` | `247x--xmJpw` |
| `_ZN3sce2np12SelectorItemC2Ev` | `925FJay6zH8` |
| `_ZN3sce2np12SelectorItemD0Ev` | `Rs0lNpdvIJo` |
| `_ZN3sce2np12SelectorItemD1Ev` | `ppCijWSMwXY` |
| `_ZN3sce2np12SelectorItemD2Ev` | `H74u5owPMbY` |
| `_ZN3sce2np12StreamBuffer10GetBufSizeEv` | `vMhV6yUYP4Q` |
| `_ZN3sce2np12StreamBuffer11GetCapacityEv` | `tTbB8Tv+l8s` |
| `_ZN3sce2np12StreamBuffer11GetDataSizeEv` | `Bq-05dBCvD4` |
| `_ZN3sce2np12StreamBuffer14CheckinForReadEm` | `NXL6DVxUVjs` |
| `_ZN3sce2np12StreamBuffer15CheckinForWriteEm` | `jFQJbHX18tA` |
| `_ZN3sce2np12StreamBuffer15CheckoutForReadEPm` | `yCD6VvrIe+o` |
| `_ZN3sce2np12StreamBuffer16CheckoutForWriteEPmb` | `tkXMJkGEvIk` |
| `_ZN3sce2np12StreamBuffer4ReadEPvm` | `WRC1YUM1vnA` |
| `_ZN3sce2np12StreamBuffer5ResetEv` | `ID-LVv24anQ` |
| `_ZN3sce2np12StreamBuffer5WriteEPKvmb` | `E8TlH0RZKqI` |
| `_ZN3sce2np12StreamBuffer6IsFullEv` | `Pd+2Es0Lx2k` |
| `_ZN3sce2np12StreamBuffer7FreeBufEv` | `uN7CJWSqBXs` |
| `_ZN3sce2np12StreamBuffer7IsEmptyEv` | `S2XusTXBJ4E` |
| `_ZN3sce2np12StreamBuffer8AllocBufEm` | `GD9Eg729Jc0` |
| `_ZN3sce2np12StreamBufferC1EP16SceNpAllocatorEx` | `1WirGSNeyxk` |
| `_ZN3sce2np12StreamBufferC2EP16SceNpAllocatorEx` | `Jm7QjcHIKg4` |
| `_ZN3sce2np12StreamBufferD1Ev` | `KqAWYOx1tvs` |
| `_ZN3sce2np12StreamBufferD2Ev` | `1PoGuVoyG3o` |
| `_ZN3sce2np12StreamBufferixEi` | `jr0OcEeQJ8o` |
| `_ZN3sce2np12WorkerThread10ThreadMainEv` | `gimH2zdBANg` |
| `_ZN3sce2np12WorkerThreadC1EPNS0_9WorkQueueE` | `YKz2oBW3ZkM` |
| `_ZN3sce2np12WorkerThreadC2EPNS0_9WorkQueueE` | `L9Ty-fG1IM4` |
| `_ZN3sce2np12WorkerThreadD1Ev` | `PvGTq9AGFfk` |
| `_ZN3sce2np12WorkerThreadD2Ev` | `+qB+WcQlMio` |
| `_ZN3sce2np13JsonDocParser5ParseEPKcm` | `4nCyBD9jBus` |
| `_ZN3sce2np13JsonDocParser5ParseEPNS0_6HandleEPNS0_12StreamReaderEPNS0_9StreamCtxE` | `fZWXFHqZ9PQ` |
| `_ZN3sce2np13JsonDocParser9GetResultEPPNS0_10JsonObjectE` | `sgh9D+MBBKA` |
| `_ZN3sce2np13JsonDocParser9GetResultEPPNS0_9JsonValueE` | `lZWmdDoBDmI` |
| `_ZN3sce2np13JsonDocParserC1EP16SceNpAllocatorExPK7JsonDef` | `-VXubTX5UK0` |
| `_ZN3sce2np13JsonDocParserC2EP16SceNpAllocatorExPK7JsonDef` | `yPmQcnrgR2Y` |
| `_ZN3sce2np13JsonDocParserD0Ev` | `p5hRe1k4Wlg` |
| `_ZN3sce2np13JsonDocParserD1Ev` | `iFOXfoXRHFQ` |
| `_ZN3sce2np13JsonDocParserD2Ev` | `xS-Hjw1psYs` |
| `_ZN3sce2np13LocalFilePathC1EP16SceNpAllocatorEx` | `RmpU8HJ4VpY` |
| `_ZN3sce2np13LocalFilePathC2EP16SceNpAllocatorEx` | `cDjiHLXPZBs` |
| `_ZN3sce2np13LocalFilePathD0Ev` | `PBlS8aRcw3o` |
| `_ZN3sce2np13LocalFilePathD1Ev` | `yKgT6-9HdQk` |
| `_ZN3sce2np13LocalFilePathD2Ev` | `-nmEECLh2hw` |
| `_ZN3sce2np14CalloutContext4InitEPKNS1_5ParamE` | `oLpLfV2Ov9A` |
| `_ZN3sce2np14CalloutContext4InitEPKcimm` | `j6CorpmdjRk` |
| `_ZN3sce2np14CalloutContext7DestroyEv` | `C282U0P6Nwg` |
| `_ZN3sce2np14CalloutContextC1Ev` | `dV+zK-Ce-2E` |
| `_ZN3sce2np14CalloutContextC2Ev` | `j4IAvbKKTzw` |
| `_ZN3sce2np14CalloutContextD2Ev` | `wY9g+hVxLTM` |
| `_ZN3sce2np14JsonDocBuilder16EscapeJsonStringEPKcPcmPm` | `cLdoHqi5Ezg` |
| `_ZN3sce2np14JsonDocBuilder23EscapeJsonStringBufSizeEPKc` | `V5xX2eroaWY` |
| `_ZN3sce2np14JsonDocBuilder5BuildEPNS0_6HandleEPNS0_12StreamWriterEPNS0_9StreamCtxE` | `xt7O5YkTU1c` |
| `_ZN3sce2np14JsonDocBuilder5BuildEPcmPm` | `irex3q-O6po` |
| `_ZN3sce2np14JsonDocBuilderD0Ev` | `wDLaq7IgfIc` |
| `_ZN3sce2np14JsonDocBuilderD1Ev` | `Kfv9jPxf7qA` |
| `_ZN3sce2np14JsonDocBuilderD2Ev` | `MH0LyghLJEE` |
| `_ZN3sce2np15CancelableScope3EndEiPKciS3_` | `pCIB7QX5e1g` |
| `_ZN3sce2np15CancelableScopeC2Ev` | `pp88xnRgJrM` |
| `_ZN3sce2np15CancelableScopeD2Ev` | `xpLjHhJBhpo` |
| `_ZN3sce2np15StreamInstaller21SetFileSizeWorkAroundEl` | `lod7OaoOhzU` |
| `_ZN3sce2np15StreamInstaller4InitEPNS0_12StreamReaderEPNS0_12StreamWriterEPNS0_9StreamCtxE` | `Dsqzl7bVBgM` |
| `_ZN3sce2np15StreamInstaller4ReadEPNS0_6HandleEPvmPm` | `mWbjmpJrclA` |
| `_ZN3sce2np15StreamInstaller7InstallEPNS0_6HandleE` | `Yu+N90bNjEo` |
| `_ZN3sce2np15StreamInstallerC1EP16SceNpAllocatorEx` | `87jf8zdIv9M` |
| `_ZN3sce2np15StreamInstallerC2EP16SceNpAllocatorEx` | `gOp3L4wFGf0` |
| `_ZN3sce2np15StreamInstallerD0Ev` | `kBDhrY67+8o` |
| `_ZN3sce2np15StreamInstallerD1Ev` | `GIfp6Vr2Lz0` |
| `_ZN3sce2np15StreamInstallerD2Ev` | `-AT9u642j7c` |
| `_ZN3sce2np16StreamReadBuffer17FillBufFromStreamEPNS0_6HandleEPNS0_12StreamReaderEmPm` | `rAFKosmR+ik` |
| `_ZN3sce2np16StreamReadBufferC1EP16SceNpAllocatorEx` | `w3QugPpYAxk` |
| `_ZN3sce2np16StreamReadBufferC2EP16SceNpAllocatorEx` | `LCk8T5b1h+4` |
| `_ZN3sce2np17StreamWriteBuffer13FlushToStreamEPNS0_6HandleEPNS0_12StreamWriterEPm` | `Ea-Yi70McNs` |
| `_ZN3sce2np17StreamWriteBuffer7FillBufEPKvmPm` | `xVE0XZYxIB4` |
| `_ZN3sce2np17StreamWriteBufferC1EP16SceNpAllocatorEx` | `QFqSZ1nyWGU` |
| `_ZN3sce2np17StreamWriteBufferC2EP16SceNpAllocatorEx` | `t17Y4epi78c` |
| `_ZN3sce2np17StreamWriteBufferD1Ev` | `6-AAhfCCzIs` |
| `_ZN3sce2np17StreamWriteBufferD2Ev` | `DegDLVNKxBw` |
| `_ZN3sce2np18HttpConnectionPool23CloseAllIdleConnectionsEv` | `jCeUP0CpiNs` |
| `_ZN3sce2np18HttpConnectionPool4InitEi` | `Vzob5RCgfnY` |
| `_ZN3sce2np18HttpConnectionPool7DestroyEv` | `iBdEFRdfpgg` |
| `_ZN3sce2np18HttpConnectionPoolC1EP16SceNpAllocatorEx` | `PznfSvchYJ8` |
| `_ZN3sce2np18HttpConnectionPoolC2EP16SceNpAllocatorEx` | `-2TYwZ4ERbM` |
| `_ZN3sce2np18HttpConnectionPoolD0Ev` | `5HWP63cOH+w` |
| `_ZN3sce2np18HttpConnectionPoolD1Ev` | `kTfkKhcdW5Y` |
| `_ZN3sce2np18HttpConnectionPoolD2Ev` | `3MVW8+eWnjs` |
| `_ZN3sce2np20BufferedStreamReader4OpenEPNS0_12StreamReaderEm` | `QGYI-e566Io` |
| `_ZN3sce2np20BufferedStreamReader4ReadEPNS0_6HandleEPvmPm` | `INZSjlRcuyQ` |
| `_ZN3sce2np20BufferedStreamReader5CloseEv` | `3Ku9r8b6gCg` |
| `_ZN3sce2np20BufferedStreamReaderC1EP16SceNpAllocatorEx` | `8A-pT35pmZQ` |
| `_ZN3sce2np20BufferedStreamReaderC2EP16SceNpAllocatorEx` | `l6s7aomzWGA` |
| `_ZN3sce2np20BufferedStreamReaderD0Ev` | `i28bR54-QFQ` |
| `_ZN3sce2np20BufferedStreamReaderD2Ev` | `PHWvRXbOnYs` |
| `_ZN3sce2np20BufferedStreamWriter4OpenEPNS0_12StreamWriterEm` | `hVmiW-7DUYw` |
| `_ZN3sce2np20BufferedStreamWriter5CloseEPNS0_6HandleE` | `PT7RZfK9zTM` |
| `_ZN3sce2np20BufferedStreamWriter5FlushEPNS0_6HandleE` | `bXUHRf4TSPU` |
| `_ZN3sce2np20BufferedStreamWriter5WriteEPNS0_6HandleEPKvmPm` | `7PCWq3UUh64` |
| `_ZN3sce2np20BufferedStreamWriterC1EP16SceNpAllocatorEx` | `pEcfn34L+oI` |
| `_ZN3sce2np20BufferedStreamWriterC2EP16SceNpAllocatorEx` | `GLNmXkhU5+k` |
| `_ZN3sce2np20BufferedStreamWriterD0Ev` | `QN2lVYwX3c8` |
| `_ZN3sce2np20BufferedStreamWriterD1Ev` | `IKstc07eVfA` |
| `_ZN3sce2np20BufferedStreamWriterD2Ev` | `MeAc+ooYzaI` |
| `_ZN3sce2np3Dir14MkdirRecursiveEPKctP16SceNpAllocatorEx` | `xF+w5MzprtY` |
| `_ZN3sce2np3Dir15CreateDirectoryEPNS0_13LocalFilePathEt` | `GKPOlf2JPTo` |
| `_ZN3sce2np3Dir15RemoveRecursiveEPKcP16SceNpAllocatorEx` | `6vXI7OZMewU` |
| `_ZN3sce2np3Dir16ConvertErrorCodeEi` | `vQH2NwKcc2Q` |
| `_ZN3sce2np3Dir4OpenEPKcj` | `k9PAEdsZOIo` |
| `_ZN3sce2np3Dir4ReadEPNS0_6DirEntEPb` | `4M2JPkb7Vbo` |
| `_ZN3sce2np3Dir5CloseEv` | `--Sj4nn7RKc` |
| `_ZN3sce2np3Dir5MkdirEPKctP16SceNpAllocatorEx` | `EecEowpLiHc` |
| `_ZN3sce2np3Dir6RemoveEPKc` | `WupK5HI1W4A` |
| `_ZN3sce2np3Dir8IsOpenedEv` | `b2+gnz4bamA` |
| `_ZN3sce2np3DirC1EP16SceNpAllocatorEx` | `aXH3Bn3WOdE` |
| `_ZN3sce2np3DirC2EP16SceNpAllocatorEx` | `96gLB4CbqDg` |
| `_ZN3sce2np3DirD0Ev` | `ar+Zz4VKvPE` |
| `_ZN3sce2np3DirD1Ev` | `sBXpmaM3PY8` |
| `_ZN3sce2np3DirD2Ev` | `O6sY-aI1EHo` |
| `_ZN3sce2np3ipc10IpmiClient10DisconnectEv` | `7dyKpPHU+Yk` |
| `_ZN3sce2np3ipc10IpmiClient16invokeSyncMethodEjPKvmPvPmm` | `r7UpNm1Po9s` |
| `_ZN3sce2np3ipc10IpmiClient4InitEPKNS2_6ConfigE` | `iRH-NE2evR4` |
| `_ZN3sce2np3ipc10IpmiClient4ctorEv` | `+EQNga+wsPc` |
| `_ZN3sce2np3ipc10IpmiClient4dtorEv` | `2h59YqPcrdM` |
| `_ZN3sce2np3ipc10IpmiClient7DestroyEv` | `+xvhXA8Ci4E` |
| `_ZN3sce2np3ipc10IpmiClientC1Ev` | `6aKYLBS8Di8` |
| `_ZN3sce2np3ipc10IpmiClientC2Ev` | `dqjlsaUX0sc` |
| `_ZN3sce2np3ipc10IpmiClientD0Ev` | `3LuoWoXJ1WI` |
| `_ZN3sce2np3ipc10IpmiClientD1Ev` | `DRbjyNom-BE` |
| `_ZN3sce2np3ipc10IpmiClientD2Ev` | `J1lpiTKAEuk` |
| `_ZN3sce2np3ipc13ServiceClientC1EPNS1_17ServiceIpmiClientEPKNS1_17ServiceClientInfoE` | `aQzxfON3l2Y` |
| `_ZN3sce2np3ipc13ServiceClientC2EPNS1_17ServiceIpmiClientEPKNS1_17ServiceClientInfoE` | `Mx6wrcdGC2w` |
| `_ZN3sce2np3ipc17ServiceIpmiClient10EndRequestEii` | `fFGPlE0oNhw` |
| `_ZN3sce2np3ipc17ServiceIpmiClient11InitServiceEi` | `G4FYQtsjOX0` |
| `_ZN3sce2np3ipc17ServiceIpmiClient11WaitRequestEiij` | `oCx3mVNvqzU` |
| `_ZN3sce2np3ipc17ServiceIpmiClient11findServiceEi` | `F2xYmg5DiR4` |
| `_ZN3sce2np3ipc17ServiceIpmiClient12BeginRequestEii` | `9aiQo-uRPJY` |
| `_ZN3sce2np3ipc17ServiceIpmiClient13DeleteRequestEii` | `cMj7li0eXgw` |
| `_ZN3sce2np3ipc17ServiceIpmiClient13IsServiceInitEi` | `oeSM31Rknck` |
| `_ZN3sce2np3ipc17ServiceIpmiClient13PollEventFlagEijmjPm` | `+ZC8QYB-BA8` |
| `_ZN3sce2np3ipc17ServiceIpmiClient13WaitEventFlagEijmjPmj` | `4GZ9O-OrfzE` |
| `_ZN3sce2np3ipc17ServiceIpmiClient14PollEventQueueEiPvm` | `q+uCQLffwQE` |
| `_ZN3sce2np3ipc17ServiceIpmiClient15CancelEventFlagEijm` | `bH08FzR5rFU` |
| `_ZN3sce2np3ipc17ServiceIpmiClient16RegisterServicesEPKNS1_17ServiceClientInfoE` | `fqAS9GQTmOU` |
| `_ZN3sce2np3ipc17ServiceIpmiClient17invokeInitServiceEi` | `BJCXJJCi0Zc` |
| `_ZN3sce2np3ipc17ServiceIpmiClient17invokeTermServiceEi` | `GuruEy9Q-Zk` |
| `_ZN3sce2np3ipc17ServiceIpmiClient18EndRequestForAsyncEii` | `8TpAxZoLLRw` |
| `_ZN3sce2np3ipc17ServiceIpmiClient19WaitRequestForAsyncEiij` | `1ONFW86TETY` |
| `_ZN3sce2np3ipc17ServiceIpmiClient20AbortRequestForAsyncEii` | `nQm4o5iOye0` |
| `_ZN3sce2np3ipc17ServiceIpmiClient20BeginRequestForAsyncEiiPN4IPMI6Client12EventNotifeeE` | `ktb6iOBLnd4` |
| `_ZN3sce2np3ipc17ServiceIpmiClient4InitEPNS2_6ConfigE` | `qFdG8Ucfeqg` |
| `_ZN3sce2np3ipc17ServiceIpmiClient4ctorEv` | `KxlKRHLf9AY` |
| `_ZN3sce2np3ipc17ServiceIpmiClient4dtorEv` | `s+dG6iqG7j0` |
| `_ZN3sce2np3ipc17ServiceIpmiClient7DestroyEv` | `IGngArGbzHo` |
| `_ZN3sce2np3ipc17ServiceIpmiClientC1Ev` | `FubuBXanVWk` |
| `_ZN3sce2np3ipc17ServiceIpmiClientC2Ev` | `zARyDXgocuk` |
| `_ZN3sce2np3ipc17ServiceIpmiClientD0Ev` | `PmsH4f3z8Yk` |
| `_ZN3sce2np3ipc17ServiceIpmiClientD1Ev` | `90XdvAqFFn8` |
| `_ZN3sce2np3ipc17ServiceIpmiClientD2Ev` | `agYDXAyL-K8` |
| `_ZN3sce2np4File8MoveDataEPNS0_6HandleElllPvm` | `alVg2J8Ssuc` |
| `_ZN3sce2np4Path10AppendStrsEiP13__va_list_tag` | `hcH2bHZ6SdI` |
| `_ZN3sce2np4Path10AppendStrsEiz` | `ahOJqm5WE4c` |
| `_ZN3sce2np4Path11BuildAppendEPcmcPKcm` | `6r6ssbPbKc4` |
| `_ZN3sce2np4Path12AddDelimiterEPcmc` | `vfBKsg+lKWc` |
| `_ZN3sce2np4Path14CheckBuildSizeEcPKcm` | `3Gae1sv2dRw` |
| `_ZN3sce2np4Path14RemoveBasenameEPcmc` | `dSWwgazWb-Q` |
| `_ZN3sce2np4Path14RemoveBasenameEv` | `yWamY9WjVII` |
| `_ZN3sce2np4Path20CheckBuildAppendSizeEPccPKcm` | `OzSl4H8NvB8` |
| `_ZN3sce2np4Path22CheckDelimiterRequiredEPKcc` | `HNBFVC+5MAI` |
| `_ZN3sce2np4Path5BuildEPcmcPKcm` | `DW+ySyerHaI` |
| `_ZN3sce2np4Path5ClearEv` | `BqFx1VLEMPk` |
| `_ZN3sce2np4Path6SetFmtEPKcz` | `kKlVoOcAGuk` |
| `_ZN3sce2np4Path6SetStrEPKc` | `+rpXQIOsHmw` |
| `_ZN3sce2np4Path6SetStrEPKcm` | `AcG6blobOQE` |
| `_ZN3sce2np4Path7SetStrsEiP13__va_list_tag` | `KQSdux7zGU4` |
| `_ZN3sce2np4Path7SetStrsEiz` | `oTBS2LGyrPo` |
| `_ZN3sce2np4Path9AppendStrEPKcm` | `j2qK6u6SL-U` |
| `_ZN3sce2np4Path9AppendStrEPKc` | `uqn3FpyF5Z8` |
| `_ZN3sce2np4Path9ExpandBufEm` | `Akr14dlHKrU` |
| `_ZN3sce2np4Path9ExpandBufEv` | `86--3NYyd1w` |
| `_ZN3sce2np4PathC1EP16SceNpAllocatorExc` | `IDjBYokUuck` |
| `_ZN3sce2np4PathC2EP16SceNpAllocatorExc` | `HxAXMrsNfiE` |
| `_ZN3sce2np4PathD0Ev` | `0fwoTW7gqfM` |
| `_ZN3sce2np4PathD1Ev` | `-3UvpBs-26g` |
| `_ZN3sce2np4Time12ParseRFC3339EPKc` | `EpJtzzWZSwE` |
| `_ZN3sce2np5NpEnv8GetNpEnvEPS1_` | `uuyEiBHghY4` |
| `_ZN3sce2np6Handle10CancelImplEi` | `-c9QK+CpQLg` |
| `_ZN3sce2np6Handle7DestroyEv` | `1atFu71dFAU` |
| `_ZN3sce2np6HandleC1Ev` | `KUJtztDMJYY` |
| `_ZN3sce2np6HandleC2Ev` | `OhpofCxYOJc` |
| `_ZN3sce2np6HandleD0Ev` | `ZOHgNNSZq4Q` |
| `_ZN3sce2np6HandleD2Ev` | `dt0A2cWjwLs` |
| `_ZN3sce2np6Thread12DoThreadMainEv` | `Ehkz-BkTPwI` |
| `_ZN3sce2np6Thread4InitEPKNS1_5ParamE` | `OoK0Ah0l1ko` |
| `_ZN3sce2np6Thread4JoinEPi` | `ne77q1GOlF8` |
| `_ZN3sce2np6Thread4ctorEv` | `3CJl5ewd7-0` |
| `_ZN3sce2np6Thread5StartEv` | `VNKdE2Dgp0Y` |
| `_ZN3sce2np6Thread7DestroyEv` | `sPti0OkVM8c` |
| `_ZN3sce2np6Thread9EntryFuncEPv` | `uphWwLZAuXA` |
| `_ZN3sce2np6Thread9GetResultEv` | `gnwCmkY-V70` |
| `_ZN3sce2np6Thread9IsRunningEv` | `qy4V8O+snLU` |
| `_ZN3sce2np6ThreadC2Ev` | `0f3ylOQJwqE` |
| `_ZN3sce2np6ThreadD0Ev` | `MEYMyfJxWXg` |
| `_ZN3sce2np6ThreadD2Ev` | `6750DaF5Pas` |
| `_ZN3sce2np7Callout5StartEjPNS1_7HandlerE` | `TDuC6To9HJ8` |
| `_ZN3sce2np7Callout5StartEmPNS1_7HandlerE` | `r0PYNWZLZS8` |
| `_ZN3sce2np7Callout9IsStartedEv` | `3ErXia+y89M` |
| `_ZN3sce2np7CalloutC1EPNS0_14CalloutContextE` | `XEXFdmQj5oI` |
| `_ZN3sce2np7CalloutC2EPNS0_14CalloutContextE` | `Bpay3NjseSU` |
| `_ZN3sce2np7CalloutD0Ev` | `Fx2UwoQVVmo` |
| `_ZN3sce2np7CalloutD1Ev` | `kUitiIVR43g` |
| `_ZN3sce2np7CalloutD2Ev` | `ebomQLbpptw` |
| `_ZN3sce2np7HttpUri5BuildEPKS1_PcmPmj` | `YtzL-Rso9bk` |
| `_ZN3sce2np7HttpUri5BuildEPKcS3_PKNS1_10QueryParamEiPcmPm` | `sDhLhhB-xlI` |
| `_ZN3sce2np7HttpUri5ParseEPS1_PKc` | `Xp92SsA5atA` |
| `_ZN3sce2np7HttpUriC1EP16SceNpAllocatorEx` | `LL9z5QvmwaA` |
| `_ZN3sce2np7HttpUriC2EP16SceNpAllocatorEx` | `q4G7qxTJWps` |
| `_ZN3sce2np7HttpUriD0Ev` | `w+C8QXqZKSw` |
| `_ZN3sce2np7HttpUriD2Ev` | `D-dT+vERWmU` |
| `_ZN3sce2np8HttpFile4OpenEPNS0_6HandleERKNS0_12HttpTemplateEPKcS8_tS8_PKNS0_9HttpTrans12TimeoutParamE` | `Ti8-pAXDJgw` |
| `_ZN3sce2np8HttpFile4OpenEPNS0_6HandleERKNS0_12HttpTemplateEPNS0_18HttpConnectionPoolEPKcSA_tSA_PKNS0_9HttpTrans12TimeoutParamE` | `YfE-VR2vYd8` |
| `_ZN3sce2np8HttpFile4ReadEPNS0_6HandleEPvmPm` | `d+xJZ63-wrc` |
| `_ZN3sce2np8HttpFile5CloseEv` | `jcPO4bt5i3o` |
| `_ZN3sce2np8HttpFileC1EP16SceNpAllocatorEx` | `tn4XsVgsb70` |
| `_ZN3sce2np8HttpFileC2EP16SceNpAllocatorEx` | `RXdPqxVnrvo` |
| `_ZN3sce2np8HttpFileD0Ev` | `T2w3ndcG-+Q` |
| `_ZN3sce2np8HttpFileD1Ev` | `6fomUWNk6Xc` |
| `_ZN3sce2np8HttpFileD2Ev` | `WAat5MtCKpc` |
| `_ZN3sce2np8JsonBool7SetBoolEb` | `FdpYFbq5C3Q` |
| `_ZN3sce2np8JsonFile4OpenEPKcit` | `9v0ZrUjk7wk` |
| `_ZN3sce2np8JsonFile5CloseEv` | `mFZezLIogNI` |
| `_ZN3sce2np8JsonFile8ReadJsonEPNS0_6HandleEPPNS0_10JsonObjectE` | `UhwdLAKPWn4` |
| `_ZN3sce2np8JsonFile9WriteJsonEPNS0_6HandleERKNS0_10JsonObjectE` | `WQa3MXlJhy0` |
| `_ZN3sce2np8JsonFileC1EP16SceNpAllocatorExPK7JsonDef` | `84AB5Si6E3E` |
| `_ZN3sce2np8JsonFileC2EP16SceNpAllocatorExPK7JsonDef` | `W0gLWfrpR+A` |
| `_ZN3sce2np8JsonFileD0Ev` | `hqPavTyQlNg` |
| `_ZN3sce2np8JsonFileD1Ev` | `wzqAM7IYGzU` |
| `_ZN3sce2np8Selector10UnregisterEPNS0_12SelectorItemE` | `CsvmrMujh20` |
| `_ZN3sce2np8Selector12GetFirstItemEv` | `YlmpqOVtAnM` |
| `_ZN3sce2np8Selector13GetEventsImplEPNS1_5EventEi` | `iy1kC+DQ+5k` |
| `_ZN3sce2np8Selector13UnregisterAllEv` | `DHtikF4iTpw` |
| `_ZN3sce2np8Selector4FindEi` | `72yKNXx+2GM` |
| `_ZN3sce2np8Selector4InitEPKc` | `5WuiSZkU3mg` |
| `_ZN3sce2np8Selector4PollEPNS1_5EventEiPi` | `SQT3-o2D9Aw` |
| `_ZN3sce2np8Selector4WaitEPNS1_5EventEiPij` | `uIix+SxGQSE` |
| `_ZN3sce2np8Selector6NotifyEPNS0_12SelectorItemE` | `yB+LINZ6x40` |
| `_ZN3sce2np8Selector8RegisterEPNS0_12SelectorItemEj` | `cSBxTr8Qvx8` |
| `_ZN3sce2np8SelectorC1EP16SceNpAllocatorEx` | `oufe5bCvXRQ` |
| `_ZN3sce2np8SelectorC2EP16SceNpAllocatorEx` | `xXopRCE2gpg` |
| `_ZN3sce2np8SelectorD0Ev` | `2HkOOhiWK3M` |
| `_ZN3sce2np8SelectorD1Ev` | `asZdig1mPlA` |
| `_ZN3sce2np8WorkItem11SetFinishedEi` | `-91vFSqiuKw` |
| `_ZN3sce2np8WorkItem14FinishCallbackEv` | `zepqHjfGe0M` |
| `_ZN3sce2np8WorkItem15RemoveFromQueueEv` | `3rGzxcMK-Mg` |
| `_ZN3sce2np8WorkItem9BindQueueEPNS0_9WorkQueueEi` | `gnh2cpEgSS8` |
| `_ZN3sce2np8WorkItemC2EPKc` | `HldN461O2Dw` |
| `_ZN3sce2np8WorkItemD1Ev` | `dnwItoXLoy4` |
| `_ZN3sce2np8WorkItemD2Ev` | `ga4OW9MGahU` |
| `_ZN3sce2np8msg_pack10MsgPackDoc3AddERKNS1_12MsgPackValueE` | `mcksYTt3a6c` |
| `_ZN3sce2np8msg_pack10MsgPackDoc5ClearEv` | `c0OAybz2W5o` |
| `_ZN3sce2np8msg_pack10MsgPackDocD0Ev` | `yXxMZ-02dNM` |
| `_ZN3sce2np8msg_pack10MsgPackDocD1Ev` | `H3xH9j+vDL4` |
| `_ZN3sce2np8msg_pack10MsgPackDocD2Ev` | `cVxiXMcEG2s` |
| `_ZN3sce2np8msg_pack10MsgPackMap10IsValidKeyERKNS1_12MsgPackValueE` | `B+kRdJjx5L8` |
| `_ZN3sce2np8msg_pack10MsgPackMap12PutAndGetPtrERKNS1_12MsgPackValueES5_PPS3_` | `MrXN6wk7gYk` |
| `_ZN3sce2np8msg_pack10MsgPackMap3PutERKNS1_12MsgPackValueES5_` | `QClFP2KKPF0` |
| `_ZN3sce2np8msg_pack10MsgPackMap6CreateEP14SceNpAllocatorPS2_` | `isr1XxY2gIc` |
| `_ZN3sce2np8msg_pack10MsgPackMap6RemoveERKNS1_12MsgPackValueEPS3_` | `6eCw3RJWCxY` |
| `_ZN3sce2np8msg_pack10MsgPackMap8AllocateEP14SceNpAllocator` | `OjjqyupeI6Q` |
| `_ZN3sce2np8msg_pack12MsgPackArray12AddAndGetPtrERKNS1_12MsgPackValueEPPS3_` | `jXZjoKUWiBQ` |
| `_ZN3sce2np8msg_pack12MsgPackArray3AddERKNS1_12MsgPackValueE` | `pqht4bHLsdk` |
| `_ZN3sce2np8msg_pack12MsgPackArray3AddEjRKNS1_12MsgPackValueE` | `1VBN-DmatAA` |
| `_ZN3sce2np8msg_pack12MsgPackArray6CreateEP14SceNpAllocatorPS2_` | `y55nRnJYB1c` |
| `_ZN3sce2np8msg_pack12MsgPackArray6RemoveEjPNS1_12MsgPackValueE` | `IvGq2makSa4` |
| `_ZN3sce2np8msg_pack12MsgPackArray8AllocateEP14SceNpAllocator` | `CuB9M1RRDOY` |
| `_ZN3sce2np8msg_pack12MsgPackValue9IsDerivedENS1_11MsgPackTypeES3_` | `aAE+32b+dCU` |
| `_ZN3sce2np8msg_pack12MsgPackValueC1ENS1_11MsgPackTypeE` | `+nifbTTTg-g` |
| `_ZN3sce2np8msg_pack12MsgPackValueC1ERKS2_` | `Wqvp6nAuan8` |
| `_ZN3sce2np8msg_pack12MsgPackValueC1Ev` | `g2rmacQqWek` |
| `_ZN3sce2np8msg_pack12MsgPackValueC2ENS1_11MsgPackTypeE` | `ubrdHLu65Pg` |
| `_ZN3sce2np8msg_pack12MsgPackValueC2ERKS2_` | `G7OZdy22jgg` |
| `_ZN3sce2np8msg_pack12MsgPackValueC2Ev` | `+FccbMW2tZ0` |
| `_ZN3sce2np8msg_pack12MsgPackValueD1Ev` | `QvQfxWPMNlQ` |
| `_ZN3sce2np8msg_pack12MsgPackValueD2Ev` | `pI5mbDNOcmw` |
| `_ZN3sce2np8msg_pack13MsgPackBinary6CreateEP14SceNpAllocatorPS2_` | `MUk+VbtOj2Y` |
| `_ZN3sce2np8msg_pack13MsgPackBinary8AllocateEP14SceNpAllocator` | `TeXWIP9m8TY` |
| `_ZN3sce2np8msg_pack13MsgPackBinary8SetValueEPKvm` | `h5bNnlNV06Y` |
| `_ZN3sce2np8msg_pack13MsgPackPacker10InitializeEPNS0_12StreamWriterEPNS0_9StreamCtxE` | `pXSEURJcnqQ` |
| `_ZN3sce2np8msg_pack13MsgPackPacker10PackBinaryEPNS0_6HandleEPKvm` | `gM9s-JYBJEI` |
| `_ZN3sce2np8msg_pack13MsgPackPacker10PackStringEPNS0_6HandleEPKcm` | `V04EbylK4Yc` |
| `_ZN3sce2np8msg_pack13MsgPackPacker10PackUInt32EPNS0_6HandleEj` | `WyDlPN5Zh0E` |
| `_ZN3sce2np8msg_pack13MsgPackPacker10PackUInt64EPNS0_6HandleEm` | `AP1XjC3ZZt8` |
| `_ZN3sce2np8msg_pack13MsgPackPacker11PackFloat32EPNS0_6HandleEf` | `Xe4VQD0rtf0` |
| `_ZN3sce2np8msg_pack13MsgPackPacker11PackFloat64EPNS0_6HandleEd` | `cg0XllwfTj8` |
| `_ZN3sce2np8msg_pack13MsgPackPacker5WriteEPNS0_6HandleEPKvm` | `lfaZ4ELD5A8` |
| `_ZN3sce2np8msg_pack13MsgPackPacker6Write1EPNS0_6HandleEh` | `mcIwbxiWNGQ` |
| `_ZN3sce2np8msg_pack13MsgPackPacker6Write2EPNS0_6HandleEt` | `0O4ZuOkfYPU` |
| `_ZN3sce2np8msg_pack13MsgPackPacker6Write4EPNS0_6HandleEj` | `1WEFyyf49dw` |
| `_ZN3sce2np8msg_pack13MsgPackPacker6Write8EPNS0_6HandleEm` | `xJsluhbPC4w` |
| `_ZN3sce2np8msg_pack13MsgPackPacker7PackMapEPNS0_6HandleEm` | `Sl4T94Sr-Oc` |
| `_ZN3sce2np8msg_pack13MsgPackPacker8FinalizeEv` | `90Tr-GIPfL8` |
| `_ZN3sce2np8msg_pack13MsgPackPacker8PackBoolEPNS0_6HandleEb` | `2ej8cH1ZkU0` |
| `_ZN3sce2np8msg_pack13MsgPackPacker9PackArrayEPNS0_6HandleEm` | `P9pyADie8NI` |
| `_ZN3sce2np8msg_pack13MsgPackPacker9PackInt32EPNS0_6HandleEi` | `IrEoEYD7Cl4` |
| `_ZN3sce2np8msg_pack13MsgPackPacker9PackInt64EPNS0_6HandleEl` | `VbLJt62pXDw` |
| `_ZN3sce2np8msg_pack13MsgPackParser10ParseArrayEPNS0_6HandleEPNS1_15MsgPackUnpackerEmPNS1_12MsgPackValueE` | `rUQbxJcILD4` |
| `_ZN3sce2np8msg_pack13MsgPackParser10ParseInt32EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE` | `ElAUhCRS+Us` |
| `_ZN3sce2np8msg_pack13MsgPackParser10ParseInt64EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE` | `o+4qe58NiK8` |
| `_ZN3sce2np8msg_pack13MsgPackParser11ParseBinaryEPNS0_6HandleEPNS1_15MsgPackUnpackerEmPNS1_12MsgPackValueE` | `OTFPfmdKsTI` |
| `_ZN3sce2np8msg_pack13MsgPackParser11ParseStringEPNS0_6HandleEPNS1_15MsgPackUnpackerEmPNS1_12MsgPackValueE` | `VX8mD5pKzRg` |
| `_ZN3sce2np8msg_pack13MsgPackParser11ParseUInt32EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE` | `QZ0S5S-2BmQ` |
| `_ZN3sce2np8msg_pack13MsgPackParser11ParseUInt64EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE` | `4MC8KYmP43A` |
| `_ZN3sce2np8msg_pack13MsgPackParser12ParseFloat32EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE` | `FtMrQNKKmsI` |
| `_ZN3sce2np8msg_pack13MsgPackParser12ParseFloat64EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE` | `bhrz+dCZFL4` |
| `_ZN3sce2np8msg_pack13MsgPackParser5ParseEPKvm` | `tGUr9CtgQ2A` |
| `_ZN3sce2np8msg_pack13MsgPackParser5ParseEPNS0_6HandleEPNS0_12StreamReaderEPNS0_9StreamCtxE` | `Av6dlMaFg1U` |
| `_ZN3sce2np8msg_pack13MsgPackParser8ParseMapEPNS0_6HandleEPNS1_15MsgPackUnpackerEmPNS1_12MsgPackValueE` | `wNsVzPWa5iw` |
| `_ZN3sce2np8msg_pack13MsgPackParser9GetResultEPPNS1_10MsgPackDocE` | `Mq-XgqBhtSY` |
| `_ZN3sce2np8msg_pack13MsgPackParser9ParseBoolEPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE` | `y4oaqmH2TDo` |
| `_ZN3sce2np8msg_pack13MsgPackParser9ParseImplEPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE` | `l4wLJeWIxNY` |
| `_ZN3sce2np8msg_pack13MsgPackParserD0Ev` | `JeZJocaJHAU` |
| `_ZN3sce2np8msg_pack13MsgPackParserD1Ev` | `ezGVzRFN7Oc` |
| `_ZN3sce2np8msg_pack13MsgPackParserD2Ev` | `O9+ZlqCjPxE` |
| `_ZN3sce2np8msg_pack13MsgPackString6CreateEP14SceNpAllocatorPS2_` | `KDqpahluouo` |
| `_ZN3sce2np8msg_pack13MsgPackString8AllocateEP14SceNpAllocator` | `lW56T9n4kQM` |
| `_ZN3sce2np8msg_pack13MsgPackString8SetValueEPKc` | `nUesWVRd6eg` |
| `_ZN3sce2np8msg_pack14MsgPackBuilder5BuildEPNS0_6HandleEPNS0_12StreamWriterEPNS0_9StreamCtxE` | `HMuylrBDF74` |
| `_ZN3sce2np8msg_pack14MsgPackBuilder5BuildEPvmPm` | `h9N+tt3BnZk` |
| `_ZN3sce2np8msg_pack14MsgPackBuilder9BuildImplEPNS0_6HandleEPNS1_13MsgPackPackerERKNS1_12MsgPackValueE` | `hontE4P4e6c` |
| `_ZN3sce2np8msg_pack14MsgPackMapImpl12PutAndGetPtrERKNS1_12MsgPackValueES5_PPS3_` | `yZBVDxWEiwc` |
| `_ZN3sce2np8msg_pack14MsgPackMapImpl3PutERKNS1_12MsgPackValueES5_` | `66vEqsQ6Row` |
| `_ZN3sce2np8msg_pack14MsgPackMapImpl6RemoveERKNS1_12MsgPackValueEPS3_` | `Dx3h0eraKUg` |
| `_ZN3sce2np8msg_pack14MsgPackMapImplC1EP14SceNpAllocator` | `0yGXiAz5POs` |
| `_ZN3sce2np8msg_pack14MsgPackMapImplC2EP14SceNpAllocator` | `dEAxAbeynUY` |
| `_ZN3sce2np8msg_pack14MsgPackMapImplD0Ev` | `rAbhCQFASus` |
| `_ZN3sce2np8msg_pack14MsgPackMapImplD1Ev` | `bvQ6yh7WuWg` |
| `_ZN3sce2np8msg_pack14MsgPackMapImplD2Ev` | `AdHs9XUPQOg` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker10InitializeEPNS0_12StreamReaderEPNS0_9StreamCtxE` | `XArFsK8+2uA` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker10UnpackBoolEPNS0_6HandleEPb` | `0SuSlL0OD1Y` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker11UnpackInt32EPNS0_6HandleEPi` | `HArGEtOilxs` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker11UnpackInt64EPNS0_6HandleEPl` | `vdKfWscHflM` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker12UnpackBinaryEPNS0_6HandleEPvm` | `YCDHCMp0sTA` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker12UnpackFormatEPNS0_6HandleEPNS1_11MsgPackTypeEPm` | `zwCONIhKweI` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker12UnpackStringEPNS0_6HandleEPcm` | `bKEdW0nRkoo` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker12UnpackUInt32EPNS0_6HandleEPj` | `6U8XYT9cnTE` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker12UnpackUInt64EPNS0_6HandleEPm` | `2tAVNch6Ufw` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker13UnpackFloat32EPNS0_6HandleEPf` | `NohPvJZLKcw` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker13UnpackFloat64EPNS0_6HandleEPd` | `3cM-L05IDCo` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker17InitInternalStateEv` | `XS6Zm+oHYtQ` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker22OnUnpackValueCompletedEv` | `DV7pXO7Yeac` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker4ReadEPNS0_6HandleEPvmPm` | `JkuKOLV3cF0` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker5Read1EPNS0_6HandleEPh` | `gtL6tUEnJz8` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker5Read2EPNS0_6HandleEPt` | `suf43BmcC5M` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker5Read4EPNS0_6HandleEPj` | `iAqkj3D4T90` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker5Read8EPNS0_6HandleEPm` | `KzfLzpQcFoE` |
| `_ZN3sce2np8msg_pack15MsgPackUnpacker8FinalizeEv` | `Yh1FQ+8DRN4` |
| `_ZN3sce2np8msg_pack16MsgPackArrayImpl16AddLastAndGetPtrERKNS1_12MsgPackValueEPPS3_` | `VYb5cgnzkes` |
| `_ZN3sce2np8msg_pack16MsgPackArrayImpl3AddEjRKNS1_12MsgPackValueE` | `UDSL5DMRF7c` |
| `_ZN3sce2np8msg_pack16MsgPackArrayImpl6RemoveEjPNS1_12MsgPackValueE` | `aYlq2zq0ELI` |
| `_ZN3sce2np8msg_pack16MsgPackArrayImpl7AddLastERKNS1_12MsgPackValueE` | `48p0z-ll3wo` |
| `_ZN3sce2np8msg_pack16MsgPackArrayImpl8AddFirstERKNS1_12MsgPackValueE` | `mVvdSTGvkTc` |
| `_ZN3sce2np8msg_pack16MsgPackArrayImplC1EP14SceNpAllocator` | `LKo7ZNBUTlU` |
| `_ZN3sce2np8msg_pack16MsgPackArrayImplC2EP14SceNpAllocator` | `s-MvauYZ7II` |
| `_ZN3sce2np8msg_pack16MsgPackArrayImplD0Ev` | `-lXuMgmNDVg` |
| `_ZN3sce2np8msg_pack16MsgPackArrayImplD1Ev` | `7LZZ7gWNBq8` |
| `_ZN3sce2np8msg_pack16MsgPackArrayImplD2Ev` | `iuilWJsw1OA` |
| `_ZN3sce2np8msg_pack16MsgPackValueImpl6AddRefEv` | `D4r++h0mvxo` |
| `_ZN3sce2np8msg_pack16MsgPackValueImpl7ReleaseEv` | `2hfOTyl0hTY` |
| `_ZN3sce2np8msg_pack17MsgPackBinaryImpl8SetValueEPKvm` | `1t979mOf5hE` |
| `_ZN3sce2np8msg_pack17MsgPackBinaryImplD0Ev` | `3Y+ZFtfwOvc` |
| `_ZN3sce2np8msg_pack17MsgPackBinaryImplD1Ev` | `ThcMErV6j54` |
| `_ZN3sce2np8msg_pack17MsgPackBinaryImplD2Ev` | `0cyGJtj6Mos` |
| `_ZN3sce2np8msg_pack17MsgPackStringImpl8SetValueEPKc` | `Lq8fO6-wUn0` |
| `_ZN3sce2np8msg_pack17MsgPackStringImplD0Ev` | `8aUdujAykDg` |
| `_ZN3sce2np8msg_pack17MsgPackStringImplD1Ev` | `WNbrJzSewnY` |
| `_ZN3sce2np8msg_pack17MsgPackStringImplD2Ev` | `72TLahYlJI4` |
| `_ZN3sce2np9HttpTrans10SetTimeoutEPKNS1_12TimeoutParamE` | `jDDvll2aQpQ` |
| `_ZN3sce2np9HttpTrans11SendRequestEPNS0_6HandleEPKvm` | `+hKyaJJCE+0` |
| `_ZN3sce2np9HttpTrans12RecvResponseEPNS0_6HandleEPvmPm` | `EhLaOnhdcXo` |
| `_ZN3sce2np9HttpTrans12SkipResponseEPNS0_6HandleE` | `fV+Q5a6p+zQ` |
| `_ZN3sce2np9HttpTrans16AddRequestHeaderEPKcS3_` | `Qfsmqs-bHeY` |
| `_ZN3sce2np9HttpTrans16SetRequestHeaderEPKcS3_` | `6bYsRATI3tQ` |
| `_ZN3sce2np9HttpTrans17GetResponseHeaderEPNS0_6HandleEPKcPvmPm` | `5qwBeeSKiSc` |
| `_ZN3sce2np9HttpTrans21SetRequestContentTypeEPKc` | `RJOlguLEy-E` |
| `_ZN3sce2np9HttpTrans23SetRequestContentLengthEm` | `ws3x3yjUyeE` |
| `_ZN3sce2np9HttpTrans4InitERKNS0_12HttpTemplateEPNS0_18HttpConnectionPoolEiPKcS8_tS8_m` | `O+FeLkOM7w0` |
| `_ZN3sce2np9HttpTrans4InitERKNS0_12HttpTemplateEPNS0_18HttpConnectionPoolEiPKcm` | `JEYp0T1VC58` |
| `_ZN3sce2np9HttpTrans4ReadEPNS0_6HandleEPvmPm` | `aWo+7jvpllY` |
| `_ZN3sce2np9HttpTrans5WriteEPNS0_6HandleEPKvmPm` | `cocNRQpq+NA` |
| `_ZN3sce2np9HttpTrans7DestroyEv` | `2e9GLlHTKA4` |
| `_ZN3sce2np9HttpTransC2EP16SceNpAllocatorEx` | `HEeXBdgvJI4` |
| `_ZN3sce2np9HttpTransD0Ev` | `Pe9fHKX7krE` |
| `_ZN3sce2np9HttpTransD1Ev` | `ls8yIODZmzc` |
| `_ZN3sce2np9JsonArray11AddItemBoolEbPPNS0_8JsonBoolE` | `3gtCC96LItc` |
| `_ZN3sce2np9JsonArray12AddItemArrayERKS1_PPS1_` | `WT19qJEfCMk` |
| `_ZN3sce2np9JsonArray13AddItemNumberEiPPNS0_10JsonNumberE` | `H00VPsPdR7s` |
| `_ZN3sce2np9JsonArray13AddItemNumberEjPPNS0_10JsonNumberE` | `cDXDQMcZWQE` |
| `_ZN3sce2np9JsonArray13AddItemNumberElPPNS0_10JsonNumberE` | `FiWBjyaPRe8` |
| `_ZN3sce2np9JsonArray13AddItemNumberEmPPNS0_10JsonNumberE` | `ovXH-Z-xE-U` |
| `_ZN3sce2np9JsonArray13AddItemObjectEPPNS0_10JsonObjectE` | `MX7crQD7X14` |
| `_ZN3sce2np9JsonArray13AddItemObjectERKNS0_10JsonObjectEPPS2_` | `OgLngPzFVqU` |
| `_ZN3sce2np9JsonArray13AddItemStringEPKcPPNS0_10JsonStringE` | `dwbx4SMFlWU` |
| `_ZN3sce2np9JsonArray5ClearEv` | `cWsZswBMjqg` |
| `_ZN3sce2np9JsonValue12AddItemValueEPS1_` | `FTMOfFYzglQ` |
| `_ZN3sce2np9JsonValue12GetItemValueEi` | `aCZjveAsynw` |
| `_ZN3sce2np9JsonValue13GetFieldValueEPKc` | `BDie4qEtKuA` |
| `_ZN3sce2np9JsonValue13GetFieldValueEiPPKc` | `aIV+HI6llz4` |
| `_ZN3sce2np9JsonValue13SetFieldValueEPKcPS1_` | `ryAZI4JvClg` |
| `_ZN3sce2np9JsonValueC2EP16SceNpAllocatorExNS1_9ValueTypeE` | `VbSIo6VAuTY` |
| `_ZN3sce2np9JsonValueD0Ev` | `LotC9rVP3Lo` |
| `_ZN3sce2np9JsonValueD1Ev` | `hBuLbn3mGBw` |
| `_ZN3sce2np9JsonValueD2Ev` | `FfSNfBmn+K8` |
| `_ZN3sce2np9LocalFile11GetFileSizeEPl` | `r8AhtDico-o` |
| `_ZN3sce2np9LocalFile11GetFileStatEPKcPNS1_8FileStatE` | `CzGEIMEefCM` |
| `_ZN3sce2np9LocalFile16ConvertErrorCodeEi` | `J7tN5iq1i60` |
| `_ZN3sce2np9LocalFile4OpenEPKcit` | `WkU1FmZoDa8` |
| `_ZN3sce2np9LocalFile4ReadEPNS0_6HandleEPvmPm` | `PsP6LYRZ7Dc` |
| `_ZN3sce2np9LocalFile4SeekEliPl` | `Flyyg6hzUOM` |
| `_ZN3sce2np9LocalFile4SyncEv` | `YtvLEI7uZRI` |
| `_ZN3sce2np9LocalFile5WriteEPNS0_6HandleEPKvmPm` | `0xL7AwgxphE` |
| `_ZN3sce2np9LocalFile6ExistsEPKcPbS4_` | `gh64pyF2-Wc` |
| `_ZN3sce2np9LocalFile6RenameEPKcS3_` | `ls4HxJ7SNOo` |
| `_ZN3sce2np9LocalFile8CopyFileEPNS0_6HandleEPKcS5_itP16SceNpAllocatorEx` | `B8ktn412thc` |
| `_ZN3sce2np9LocalFile8ReadFileEPNS0_6HandleEPKcPvPm` | `0vorueuLY6w` |
| `_ZN3sce2np9LocalFileC2Ev` | `HP4jsVYqBKg` |
| `_ZN3sce2np9LocalFileD1Ev` | `3eoh4hjcYag` |
| `_ZN3sce2np9LocalFileD2Ev` | `s-C88O6Y8iU` |
| `_ZN3sce2np9WorkQueue16WorkItemFinishedEPNS0_8WorkItemEi` | `UYAD7sUQcYU` |
| `_ZN3sce2np9WorkQueue17ProcFinishedItemsENS0_14WorkItemStatusE` | `-9cU3y6rXVM` |
| `_ZN3sce2np9WorkQueue18RemoveFinishedItemEPNS0_8WorkItemE` | `ovc4ZvD0YjY` |
| `_ZN3sce2np9WorkQueue18WaitForPendingItemEPPNS0_8WorkItemEPb` | `vPju3W13byw` |
| `_ZN3sce2np9WorkQueue4InitEPKNS0_6Thread5ParamE` | `U0YoWwgg8aI` |
| `_ZN3sce2np9WorkQueue4InitEPKcimm` | `+dGO+GS2ZXQ` |
| `_ZN3sce2np9WorkQueue4StopEv` | `4DE+nnCVRPA` |
| `_ZN3sce2np9WorkQueue4dtorEv` | `wESN-qrVhOU` |
| `_ZN3sce2np9WorkQueue5StartEv` | `VnQolo6vTr4` |
| `_ZN3sce2np9WorkQueue6CancelEii` | `laqZEULcfgw` |
| `_ZN3sce2np9WorkQueue6IsInitEv` | `CznMfhTIvVY` |
| `_ZN3sce2np9WorkQueue7DestroyEv` | `NeopmYshD0U` |
| `_ZN3sce2np9WorkQueue7EnqueueEiPNS0_8WorkItemE` | `KQSxXJBepQ4` |
| `_ZN3sce2np9WorkQueue9CancelAllEi` | `zmOmSLnqlBQ` |
| `_ZN3sce2np9WorkQueue9IsRunningEv` | `eTy3L1azX4E` |
| `_ZN3sce2np9WorkQueueC1Ev` | `X6NVkdpRnog` |
| `_ZN3sce2np9WorkQueueC2Ev` | `p+bd65J177I` |
| `_ZN3sce2np9WorkQueueD0Ev` | `uyNO0GnFhPw` |
| `_ZN3sce2np9WorkQueueD1Ev` | `1QFKnDJxk3A` |
| `_ZN3sce2npeqERK27SceNpCommunicationSignatureRKNS0_10NpCommSignE` | `gLdk9PG4cEI` |
| `_ZN3sce2npeqERKNS0_10NpCommSignERK27SceNpCommunicationSignature` | `LM15YX7BCnU` |
| `_ZN3sce2npeqERKNS0_10NpCommSignES3_` | `SQLr0ZomMUk` |
| `_ZN3sce2npneERK27SceNpCommunicationSignatureRKNS0_10NpCommSignE` | `LcpagIBUTpU` |
| `_ZN3sce2npneERKNS0_10NpCommSignERK27SceNpCommunicationSignature` | `27UI+hudqPc` |
| `_ZN3sce2npneERKNS0_10NpCommSignES3_` | `zwA76Qy+Gic` |
| `_ZNK3sce2np10Cancelable6IsInitEv` | `KyDWNwpREH4` |
| `_ZNK3sce2np10EventQueue7IsEmptyEv` | `jxPY-0x8e-M` |
| `_ZNK3sce2np10JsonNumber10CloneValueEP16SceNpAllocatorEx` | `GSOwA5SK9H4` |
| `_ZNK3sce2np10JsonNumber5CloneEP16SceNpAllocatorEx` | `COxqqhvLSyM` |
| `_ZNK3sce2np10JsonNumber6GetNumEPi` | `Sk8AdNQUDm8` |
| `_ZNK3sce2np10JsonNumber6GetNumEPj` | `nHgo2VpnCB8` |
| `_ZNK3sce2np10JsonNumber6GetNumEPl` | `Agsyrf4L8uA` |
| `_ZNK3sce2np10JsonNumber6GetNumEPm` | `P2cGbJ5nD1w` |
| `_ZNK3sce2np10JsonNumber9GetNumStrEv` | `GAGrrgl7GAU` |
| `_ZNK3sce2np10JsonObject10CloneValueEP16SceNpAllocatorEx` | `HgPgJOJsGn8` |
| `_ZNK3sce2np10JsonObject12GetFieldBoolEPKc` | `jmGPUJmU+tc` |
| `_ZNK3sce2np10JsonObject12GetFieldBoolEiPPKc` | `kPnWBn-uzAU` |
| `_ZNK3sce2np10JsonObject13GetFieldArrayEPKc` | `VkLf6Cr0MUM` |
| `_ZNK3sce2np10JsonObject13GetFieldArrayEiPPKc` | `VHD+kMJc3Uw` |
| `_ZNK3sce2np10JsonObject13GetFieldCountEv` | `cGNF3NpbpE0` |
| `_ZNK3sce2np10JsonObject14GetFieldNumberEPKcPi` | `QFgm3bSuU44` |
| `_ZNK3sce2np10JsonObject14GetFieldNumberEPKcPj` | `gL9pFDitAIs` |
| `_ZNK3sce2np10JsonObject14GetFieldNumberEPKcPl` | `dgl5P1mHxvc` |
| `_ZNK3sce2np10JsonObject14GetFieldNumberEPKcPm` | `Cpk3wB7yE3U` |
| `_ZNK3sce2np10JsonObject14GetFieldNumberEPKc` | `ceRnvbGHEdA` |
| `_ZNK3sce2np10JsonObject14GetFieldNumberEiPPKc` | `5N21NQ+ltTg` |
| `_ZNK3sce2np10JsonObject14GetFieldObjectEPKc` | `Yl+ccBY0b04` |
| `_ZNK3sce2np10JsonObject14GetFieldObjectEiPPKc` | `dhbtAbBHaao` |
| `_ZNK3sce2np10JsonObject14GetFieldStringEPKcPcm` | `GbUz2kxZpTI` |
| `_ZNK3sce2np10JsonObject14GetFieldStringEPKc` | `pJt0SbTd5pw` |
| `_ZNK3sce2np10JsonObject14GetFieldStringEiPPKc` | `qPK7e4FXQKE` |
| `_ZNK3sce2np10JsonObject5CloneEP16SceNpAllocatorEx` | `k1VlSDg9azo` |
| `_ZNK3sce2np10JsonString10CloneValueEP16SceNpAllocatorEx` | `HFWZt3mZCkM` |
| `_ZNK3sce2np10JsonString5CloneEP16SceNpAllocatorEx` | `2dAzrh4J0WY` |
| `_ZNK3sce2np10JsonString6GetStrEv` | `RIPYdMx8wZw` |
| `_ZNK3sce2np10MemoryFile8IsOpenedEv` | `6VkBExKNVeA` |
| `_ZNK3sce2np10NpCommSign6EqualsEPhm` | `zRslK77fW1M` |
| `_ZNK3sce2np10NpCommSign6EqualsERK27SceNpCommunicationSignature` | `dk+HPZGhJNg` |
| `_ZNK3sce2np10NpCommSign6EqualsERKS1_` | `3FnJuHC3KaI` |
| `_ZNK3sce2np10NpCommSign7GetDataEPhm` | `ecNwTNzVnlc` |
| `_ZNK3sce2np10NpCommSign7IsEmptyEv` | `o2KW4iadRrw` |
| `_ZNK3sce2np12HttpTemplate6IsInitEv` | `2mrb4euDuGY` |
| `_ZNK3sce2np18HttpConnectionPool6IsInitEv` | `9cJ2S8f4Mj4` |
| `_ZNK3sce2np20BufferedStreamReader8IsOpenedEv` | `efokR7Xz8MQ` |
| `_ZNK3sce2np20BufferedStreamWriter8IsOpenedEv` | `ek1vZf9hlaU` |
| `_ZNK3sce2np3ipc10IpmiClient6IsInitEv` | `GI-kJrlbsDQ` |
| `_ZNK3sce2np3ipc17ServiceIpmiClient6IsInitEv` | `C8SMKrvRkl8` |
| `_ZNK3sce2np4Path6GetStrEv` | `8qEFhKvl2Cw` |
| `_ZNK3sce2np4Path7IsEmptyEv` | `d-LQfrbYBuY` |
| `_ZNK3sce2np6Handle6IsInitEv` | `Z68UCklzmHs` |
| `_ZNK3sce2np6Thread6IsInitEv` | `fK5e+K6kSBg` |
| `_ZNK3sce2np8HttpFile8IsOpenedEv` | `bWwPth5tBxU` |
| `_ZNK3sce2np8JsonBool10CloneValueEP16SceNpAllocatorEx` | `2msnT+vCZmo` |
| `_ZNK3sce2np8JsonBool5CloneEP16SceNpAllocatorEx` | `nxWdtBJF070` |
| `_ZNK3sce2np8JsonBool7GetBoolEv` | `Bd7bMuLrAqE` |
| `_ZNK3sce2np8JsonFile8IsOpenedEv` | `RAn2C9q8ZeE` |
| `_ZNK3sce2np8JsonNull10CloneValueEP16SceNpAllocatorEx` | `7lonFwHbM8A` |
| `_ZNK3sce2np8JsonNull5CloneEP16SceNpAllocatorEx` | `hYQzNpYaClY` |
| `_ZNK3sce2np8msg_pack10MsgPackDoc18GetMessagePackSizeEv` | `2GCKkDEZ10Y` |
| `_ZNK3sce2np8msg_pack10MsgPackMap11ContainsKeyERKNS1_12MsgPackValueE` | `LXT3wP+bXpw` |
| `_ZNK3sce2np8msg_pack10MsgPackMap3GetERKNS1_12MsgPackValueEPS3_` | `Q8zIb0yTAmo` |
| `_ZNK3sce2np8msg_pack10MsgPackMap6GetPtrERKNS1_12MsgPackValueEPPS3_` | `yHjqkRTF5JA` |
| `_ZNK3sce2np8msg_pack10MsgPackMap7GetSizeEv` | `C76Kms3ZD98` |
| `_ZNK3sce2np8msg_pack10MsgPackMap8IteratorEv` | `AnxdSIcTprM` |
| `_ZNK3sce2np8msg_pack10MsgPackMap9CloneDeepEP14SceNpAllocatorPNS1_12MsgPackValueE` | `Esx6v78xYY8` |
| `_ZNK3sce2np8msg_pack12MsgPackArray3GetEjPNS1_12MsgPackValueE` | `w1Dwk1H21rU` |
| `_ZNK3sce2np8msg_pack12MsgPackArray6GetPtrEjPPNS1_12MsgPackValueE` | `+Xh8+oc4Nvs` |
| `_ZNK3sce2np8msg_pack12MsgPackArray7GetSizeEv` | `sRo-6l5NnqQ` |
| `_ZNK3sce2np8msg_pack12MsgPackArray8IteratorEv` | `Kfe6nDcyy0c` |
| `_ZNK3sce2np8msg_pack12MsgPackArray9CloneDeepEP14SceNpAllocatorPNS1_12MsgPackValueE` | `P-PCWLpRblg` |
| `_ZNK3sce2np8msg_pack12MsgPackValue12ToJsonStringEPcmPmi` | `5Uv-b7crx74` |
| `_ZNK3sce2np8msg_pack12MsgPackValue13IsDerivedFromENS1_11MsgPackTypeE` | `fHf8cHUKMmY` |
| `_ZNK3sce2np8msg_pack12MsgPackValue13IsDerivedFromERKS2_` | `iXsHViCTZls` |
| `_ZNK3sce2np8msg_pack12MsgPackValue16IsRefDerivedFromERKS2_` | `42gvQ-33bFg` |
| `_ZNK3sce2np8msg_pack12MsgPackValue18GetMessagePackSizeEv` | `iumXkJgxszE` |
| `_ZNK3sce2np8msg_pack12MsgPackValue5CloneEP14SceNpAllocatorPS2_` | `qT9kwGpvc5c` |
| `_ZNK3sce2np8msg_pack12MsgPackValue6EqualsERKS2_` | `-S2vvy5A7uc` |
| `_ZNK3sce2np8msg_pack13MsgPackBinary8GetValueEPPKvPm` | `RWPHCuxnU4I` |
| `_ZNK3sce2np8msg_pack13MsgPackBinary9CloneDeepEP14SceNpAllocatorPNS1_12MsgPackValueE` | `nA6u6ucFqNs` |
| `_ZNK3sce2np8msg_pack13MsgPackString8GetValueEv` | `yxNbMNBjm4M` |
| `_ZNK3sce2np8msg_pack13MsgPackString9CloneDeepEP14SceNpAllocatorPNS1_12MsgPackValueE` | `Ui-ySjXmcpE` |
| `_ZNK3sce2np8msg_pack14MsgPackMapImpl11ContainsKeyERKNS1_12MsgPackValueE` | `65i-XELUp+s` |
| `_ZNK3sce2np8msg_pack14MsgPackMapImpl12ToJsonStringEPcmPmi` | `gJboH-ryTkY` |
| `_ZNK3sce2np8msg_pack14MsgPackMapImpl18GetMessagePackSizeEv` | `fuNOUJlwmzI` |
| `_ZNK3sce2np8msg_pack14MsgPackMapImpl3GetERKNS1_12MsgPackValueEPS3_` | `myP3tLf3IIE` |
| `_ZNK3sce2np8msg_pack14MsgPackMapImpl6EqualsERKNS1_16MsgPackValueImplE` | `vufV0Jir9yg` |
| `_ZNK3sce2np8msg_pack14MsgPackMapImpl6GetPtrERKNS1_12MsgPackValueEPPS3_` | `blPtTAiypSE` |
| `_ZNK3sce2np8msg_pack14MsgPackMapImpl8GetEntryERKNS1_12MsgPackValueE` | `Y20qmf0eays` |
| `_ZNK3sce2np8msg_pack16MsgPackArrayImpl12ToJsonStringEPcmPmi` | `W17sI2kKub0` |
| `_ZNK3sce2np8msg_pack16MsgPackArrayImpl14CreateNewEntryERKNS1_12MsgPackValueEPPNS2_5EntryE` | `w-Xa1Pufw0A` |
| `_ZNK3sce2np8msg_pack16MsgPackArrayImpl18GetMessagePackSizeEv` | `YG4UFVA8NNI` |
| `_ZNK3sce2np8msg_pack16MsgPackArrayImpl3GetEjPNS1_12MsgPackValueE` | `UIx+jN0oHKo` |
| `_ZNK3sce2np8msg_pack16MsgPackArrayImpl6EqualsERKNS1_16MsgPackValueImplE` | `LNi2lxasBmc` |
| `_ZNK3sce2np8msg_pack16MsgPackArrayImpl6GetPtrEjPPNS1_12MsgPackValueE` | `DncJS3dQyzc` |
| `_ZNK3sce2np8msg_pack16MsgPackArrayImpl8GetEntryEjPPNS2_5EntryE` | `zCudJerqqx0` |
| `_ZNK3sce2np8msg_pack17MsgPackBinaryImpl12ToJsonStringEPcmPmi` | `gdCv0AhNMno` |
| `_ZNK3sce2np8msg_pack17MsgPackBinaryImpl18GetMessagePackSizeEv` | `Em8AceEcrEY` |
| `_ZNK3sce2np8msg_pack17MsgPackBinaryImpl6EqualsERKNS1_16MsgPackValueImplE` | `mLomEr7yONY` |
| `_ZNK3sce2np8msg_pack17MsgPackBinaryImpl8GetValueEPPKvPm` | `BTCx0nYRQkg` |
| `_ZNK3sce2np8msg_pack17MsgPackStringImpl12ToJsonStringEPcmPmi` | `iz2atGaNrss` |
| `_ZNK3sce2np8msg_pack17MsgPackStringImpl18GetMessagePackSizeEv` | `PgQV4Wfercc` |
| `_ZNK3sce2np8msg_pack17MsgPackStringImpl6EqualsERKNS1_16MsgPackValueImplE` | `PKN5Bs2wXzs` |
| `_ZNK3sce2np9HttpTrans6IsInitEv` | `dGuEML7cY1k` |
| `_ZNK3sce2np9JsonArray10CloneValueEP16SceNpAllocatorEx` | `wcIp-uD9YPo` |
| `_ZNK3sce2np9JsonArray11GetItemBoolEi` | `rfoEqFVBpP4` |
| `_ZNK3sce2np9JsonArray12GetItemArrayEi` | `sQDczYjVxz0` |
| `_ZNK3sce2np9JsonArray12GetItemCountEv` | `fVmIx0jQoF8` |
| `_ZNK3sce2np9JsonArray13GetItemNumberEi` | `ReZjcCGb0F4` |
| `_ZNK3sce2np9JsonArray13GetItemObjectEi` | `AbxVvcXAra0` |
| `_ZNK3sce2np9JsonArray13GetItemStringEi` | `s6thopb23cg` |
| `_ZNK3sce2np9JsonArray5CloneEP16SceNpAllocatorEx` | `EcboqmwkrMY` |
| `_ZNK3sce2np9JsonValue11GetJsonBoolEv` | `Tqk1BXdRO00` |
| `_ZNK3sce2np9JsonValue11GetJsonNullEv` | `PtOJ24KA7WU` |
| `_ZNK3sce2np9JsonValue12GetItemValueEi` | `JcAsZlyr3Mo` |
| `_ZNK3sce2np9JsonValue12GetJsonArrayEv` | `xdyRytch1ig` |
| `_ZNK3sce2np9JsonValue13GetFieldValueEPKc` | `oapD46ePb2I` |
| `_ZNK3sce2np9JsonValue13GetFieldValueEiPPKc` | `edoLuiE1FUU` |
| `_ZNK3sce2np9JsonValue13GetJsonNumberEv` | `cVO9dqU6oBI` |
| `_ZNK3sce2np9JsonValue13GetJsonObjectEv` | `mk5Lk4zIrTk` |
| `_ZNK3sce2np9JsonValue13GetJsonStringEv` | `Pwx-bAw1SH0` |
| `_ZNK3sce2np9LocalFile8IsOpenedEv` | `LmnydDznzlc` |
| `_ZThn16_N3sce2np10MemoryFile5WriteEPNS0_6HandleEPKvmPm` | `QwO4sr6XzSY` |
| `_ZThn16_N3sce2np10MemoryFileD0Ev` | `ojBk-UJxzWw` |
| `_ZThn16_N3sce2np9HttpTransD1Ev` | `GG1Y+vBUkdU` |
| `_ZThn16_N3sce2np9LocalFile5WriteEPNS0_6HandleEPKvmPm` | `+3ySpB1buMs` |
| `_ZThn16_N3sce2np9LocalFileD0Ev` | `hSnLhjGefsU` |
| `_ZThn16_N3sce2np9LocalFileD1Ev` | `q3s6++iIzjE` |
| `_ZThn8_N3sce2np10MemoryFileD0Ev` | `7bzUdBtIQhE` |
| `_ZThn8_N3sce2np10MemoryFileD1Ev` | `lNs-oTKpG9s` |
| `_ZThn8_N3sce2np6Handle10CancelImplEi` | `xDrWJARfCbk` |
| `_ZThn8_N3sce2np6HandleD0Ev` | `YqMS-iAjFY8` |
| `_ZThn8_N3sce2np9HttpTrans4ReadEPNS0_6HandleEPvmPm` | `G+v692ul7MA` |
| `_ZThn8_N3sce2np9HttpTransD0Ev` | `sGhCzaJf+jQ` |
| `_ZThn8_N3sce2np9HttpTransD1Ev` | `PUqCtFwnNvA` |
| `_ZThn8_N3sce2np9LocalFile4ReadEPNS0_6HandleEPvmPm` | `NtsHoOq2ao4` |
| `_ZThn8_N3sce2np9LocalFileD0Ev` | `Gh35wbyg4U8` |
| `_sceNpAllocatorFree` | `BztTl7QeYqE` |
| `_sceNpAllocatorRealloc` | `VWcTu8wKwlQ` |
| `_sceNpAllocatorStrdup` | `c8-4aC9opYE` |
| `_sceNpAllocatorStrndup` | `vqA9bl6WsF0` |
| `_sceNpFree` | `z5kwfM5InpI` |
| `_sceNpHeapFree` | `p1vvpKGRXe4` |
| `_sceNpHeapRealloc` | `wsfyvM+VbUk` |
| `_sceNpHeapStrdup` | `atWcfgasESY` |
| `_sceNpHeapStrndup` | `RzLv+HR5E2A` |
| `_sceNpMalloc` | `w2+qV1RJgcI` |
| `_sceNpMsgPackStrUtilIsValidUtf8StringWithSize` | `YSFA6O6aaT4` |
| `_sceNpMsgPackStrUtilIsValidUtf8String` | `ysX96PgNe2U` |
| `_sceNpRealloc` | `UmzxltBpiiY` |
| `sceNpJsonParse2ForNpExt` | `I4shXv-fPTA` |
| `sceNpJsonParse2ForNpExtInit` | `IFacEHxssIw` |
| `sceNpJwsHs256Decode` | `Ohe4hbpISbY` |
| `sceNpJwsHs256Encode` | `V4km6-iqbL8` |
| `sceNpJwsHs256GetDecodeSize` | `Cuw8NCrme3w` |
| `sceNpJwsHs256GetEncodeSize` | `t8TnW+lPMfM` |
| `sceNpUtilNpIdToJid` | `KyB1IAY2BiU` |
| `sceNpUtilNumChars` | `c+ssxRf1Si0` |
| `sceNpUtilSerializeJid` | `okX7IjW0QsI` |
| `sceNpUtilWebApiCheckResponseHeaderAndShowNotification` | `gHF5cBwI8Gk` |
| `sceNpUtilWebApiEnableNpDebug` | `NBVRUlE8k64` |
| `sceNpUtilXmlGetOneChar` | `Ls4eWDrbNmg` |
| `sceNpXmlParseInit` | `MCLGkfBmw4c` |
| `Unknown` | `5FuxkbSbLtk` |
| `Unknown` | `IMWAoxm9nAw` |
| `Unknown` | `NOTv-472yf4` |
| `Unknown` | `NnxHmyZODbk` |
| `Unknown` | `WF2l-GUIlrw` |
| `Unknown` | `WaBg+f4DGk8` |
| `Unknown` | `qlYEhYUyESQ` |
| `Unknown` | `wnXVM2Zy49s` |
| `Unknown` | `xJecuUi348c` |

### Unknown function info (continued)

- [sceNpUtilGetWebApi2FakeRateLimit](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `pmHBFJyju9E`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetWebApi2FakeRateLimitTarget](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `ZRxKp9vjcNc`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilGetWebTraceSetting](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `4CqfNm3pisU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilJidToNpId](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (NID `458yjI+OECI`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.

### Functional

- [Online ID and NpId utilities](../../../core/libs/prx/libSceNpCommon/Export.cpp) (libSceNpCommon) - `sceNpCmpOnlineId`, `sceNpCmpNpIdInOrder`, `sceNpGetPlatformType` and `sceNpIntIsValidOnlineId` are not measured on a console; their signatures, checks and results follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/a808e76/src/core/libraries/np/np_common.cpp), including the platform tags read from `opt[4..7]` and the 3 to 16 character online ID rule.
- [Mutex, condition and thread wrappers](../../../core/libs/prx/libSceNpCommon/Export.cpp) (libSceNpCommon) - the `sceNpMutex*`, `sceNpLwMutex*`, `sceNpCond*` and `sceNpJoinThread` wrappers forward to the libkernel `scePthread*` calls as in [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/a808e76/src/core/libraries/np/np_common.cpp) and are not measured on a console. Only `EBUSY` from try-lock and `ETIMEDOUT` from a timed wait are mapped to NP errors; every other kernel error is returned unchanged. The `flags` argument of `sceNpCondInit` is ignored.
- [sceNpGetSdkVersion](../../../core/libs/prx/libSceNpCommon/Export.cpp) (libSceNpCommon) - writes the libkernel system software version as `major.minor` into an 8-byte buffer without checking it, as shadPS4 does; that version is the fixed `1.00`, not the SDK version a title was built with.
- [The exports in Runtime.cpp](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) and [Strings.cpp](../../../core/libs/prx/libSceNpCommon/Strings.cpp) follow `libSceNpCommon.sprx` from firmware 13.60, read from disassembly and not measured on a console.
- [The sce::np::Object new and delete operators](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) (libSceNpCommon) - do not check that the size plus their 16-byte header fits, and free through the allocator stored in that header, ignoring the allocator passed to the delete forms, as `libSceNpCommon.sprx` of firmware 13.60 does, read from disassembly, not measured on a console. The extended forms keep the address of the `SceNpAllocatorEx` itself, whose first member is the allocator.
- [sce::np::MemoryStreamReader and MemoryStreamWriter](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) (libSceNpCommon) - follow `libSceNpCommon.sprx` of firmware 13.60, read from disassembly, not measured on a console: `Read` always writes the count through its output pointer, while `Write` checks it for null; a writer with a null buffer only counts, with no limit; the room left is `size - position` unsigned, except that a size of 0 gives no room, so a position past a non-zero size is not refused; the handle argument is ignored.
- [sce::np::NpTitleId Parse and ConstructFromTitleId](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) (libSceNpCommon) - drop the parsed `_NN` suffix and store `_00`, and accept only ASCII letters and digits, as `libSceNpCommon.sprx` of firmware 13.60 does, read from disassembly, not measured on a console. The module classifies characters with the C-locale `_Getpctype` table, so a locale with other letters is not covered. Parse takes exactly 12 characters, ConstructFromTitleId exactly 9, and a rejected title id leaves the object unchanged.
- [sceNpCreateThread](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) always applies an explicit priority when one is given; the firmware picks between explicit and inherited scheduling from the title's compiled SDK version, which is not available here.
- [sceNpCalloutStartOnCtx and sceNpCalloutStartOnCtx64](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) look for the callout in the queue before writing it and return `0x80558006` with the entry and the queue unchanged. Firmware 13.60, read from disassembly and not measured on a console, clears and fills the entry before it takes the lock and checks the queue, so restarting a queued callout returns the same error but drops every callout queued after it.
- [sceNpArchInit and sceNpArchTerm](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) keep a reference count under a lock and create the global heap once. Firmware 13.60, read from disassembly and not measured on a console, uses no lock and never sets its count on the first call, so every call made while the count is zero creates the heap again, and a call to sceNpArchTerm at zero destroys the heap and leaves the count negative.
- [sceNpGetRandom](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) draws from a 32-bit Mersenne Twister seeded once per process; the console uses SFMT-19937 with a seed set inside the module, so the sequences differ.
- [sceNpHeapShowStat](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) and [sceNpDbgDumpBinary](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) print the same information as the console in a different layout.
- [The string-to-integer exports](../../../core/libs/prx/libSceNpCommon/Strings.cpp) clear `errno` before parsing; the firmware reads whatever `errno` held before the call when the result saturates.
- [The integer-to-string exports](../../../core/libs/prx/libSceNpCommon/Strings.cpp) return `0x80558008` when the hexadecimal prefix does not fit; the firmware writes the prefix without checking the size. [The base64 decoders](../../../core/libs/prx/libSceNpCommon/Strings.cpp) treat bytes above 0x7F as invalid; the firmware indexes its table with them as signed values. [sceNpUtilJsonGetOneChar](../../../core/libs/prx/libSceNpCommon/Strings.cpp) checks that a `\u` escape and a surrogate pair fit in the input; the firmware reads past it.
- [sceNpUtilGetNpDebug, sceNpUtilGetNpTestPatch, sceNpUtilGetTrcNotify, sceNpUtilGetIgnoreNpTitleId, sceNpUtilGetFakeDisplayNameMode, sceNpUtilGetWebTraceSetting, sceNpUtilGetWebApi2FakeRateLimit, sceNpUtilGetWebApi2FakeRateLimitTarget, sceNpUtilGetFakeRateLimit, sceNpUtilGetDbgCommerce, sceNpUtilGetSystemLanguage, sceNpUtilGetEnv, sceNpUtilGetDateSetAuto and sceNpUtilGetShareTitleCheck](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) read system and debug registry keys through libSceRegMgr, whose `sceRegMgrNonSysGetInt` and string reads still throw; they throw until those exist.
- [sceNpOpenSema and sceNpCloseSema](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) forward to the libkernel open and close calls for named semaphores, which still throw.
- [sceNpSetPlatformType, sceNpIntGetPlatformType, sceNpIntSetPlatformType, sceNpUtilCanonicalizeNpIdForPs4, sceNpUtilCanonicalizeNpIdForPsp2, sceNpUtilJidToNpId and sceNpGetNavSdkVersion](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) depend on console platform tags, network domain names or a version string of the console; they throw.
- [sceNpJsonParse, sceNpJsonParseInit, sceNpJsonParseBuf, sceNpJsonParseBufInit, sceNpJsonParseEx, sceNpJsonParseExInit and the sceNpServerErrorJson exports](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) are a streaming, schema-checked JSON parser whose event and error rules are not yet specified; they throw.
- [_ZN3sce2np10CancelableC2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `vqekW3s-eFg`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilXmlEscape](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `5bBPLZV49kY`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np4CondD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `AKiHGWhC2KU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_sceNpAllocatorExRealloc](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `kKF3w-XkCWA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np8WorkItem6CancelEi](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Oq5aepLkEWg`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9WorkQueue11GetItemByIdEi](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `F2umEBpQFHc`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np14JsonDocBuilderC2ERKNS0_9JsonValueE](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `dhJGQPKLmn0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np7RingBuf4dtorEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `rvz8xYxhMW0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZThn16_N3sce2np9HttpTransD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `zWIFe+d77PU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np10MemoryFileD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `6Vst7HqJMXU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np10EventQueueC2EP16SceNpAllocatorEx](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `esiO4He2WTU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np4Cond4ctorEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `n9pzAHeCCVU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np16StreamReadBufferD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `ZufKqNXItD0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZNK3sce2np9EventFlag6IsInitEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `st-oQvV7HVI`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np10CancelLockC2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `MecB8wAHCfE`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np13NpTitleSecretC2ERK16SceNpTitleSecret](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `9QN7g5mQgCU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np8WorkItem10SetRunningEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `XUCjhejJvPc`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np3ipc17ServiceIpmiClient21CreateRequestForAsyncEPiiPKvm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `v5Z2LAKua28`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np18HttpConnectionPool13InvalidateAllEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `et05S+nkWG8`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np3ipc17ServiceIpmiClient21DeleteRequestForAsyncEii](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `7oJpAd+vJQA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np15CancelableScopeD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `km5-rjNjSFk`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np8JsonFileD2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `QFYVZvAJNC8`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9JsonArray12AddItemArrayEPPS1_](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `4cIJxNKQK5g`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np14CalloutContextD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `WR4mjQeqz6s`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9Semaphore4WaitEj](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `hQLw6eE4O44`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2npeqERKNS0_4TimeES3_](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `UVLmT9lzRYA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np13RingBufMemory6ExpandEm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `do0t--lEKMM`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZThn8_N3sce2np10MemoryFile4ReadEPNS0_6HandleEPvmPm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `E6GYo9uzjds`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np7RingBuf15CheckoutForReadEPm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `d8NGGmSEFfU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9EventFlag4OpenEPKc](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `5Wy+JxpCBxg`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np13NpTitleSecret5ClearEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `X0vEo7cZamA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np10EventQueue7EnqueueEPKvmj](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `AV5jHo8O3+E`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np12HttpTemplateD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Bm74HLvoNY4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9WorkQueueD2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `AIDhc3KCK7w`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZThn16_N3sce2np9HttpTrans5WriteEPNS0_6HandleEPKvmPm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `eRlqlofFKYg`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np10EventQueue4ctorEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `xu9qWN0YYC4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np12StreamReader4ReadEPNS0_6HandleEPNS0_9StreamCtxEPvmPm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `rCRh3V03bPs`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np20BufferedStreamReaderD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Tgr66MThOxA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np3ipc10IpmiClient7ConnectEPKvm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `CGKtxL26XqI`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np4Cond4InitEPKcPNS0_5MutexE](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `wWTqVcTnep8`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np3ipc17ServiceIpmiClient10DisconnectEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `uvYTUK5xYG8`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np3ipc10IpmiClient11IsConnectedEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `prj9aMR74bA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np12HttpTemplate19SetAuthInfoCallbackEPFii15SceHttpAuthTypePKcPcS5_iPPhPmPiPvESA_](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `ezJnmv7hkAg`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilParseJid](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `oz2SlXNAnuI`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9Semaphore6CreateEiiPKc](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `wcOCedFKan4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2npleERKNS0_4TimeERK10SceRtcTick](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `cAeFZE72SXU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpUtilXmlUnescape](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `+0rj9KhmYb0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [sceNpXmlParse](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `fEcrs9UPPyo`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9WorkQueue15GetFinishedItemENS0_14WorkItemStatusE](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `wM4q1JMisvA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9EventFlag4PollEmjPm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `37Rd2JS+FCM`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9EventFlagD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `uHOOEbuzjEQ`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9LocalFile6RemoveEPKc](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `haDbtVOmaao`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np14JsonDocBuilder12BuildBufSizeEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `PYBehFWVd60`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np10MemoryFile8TruncateEl](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `1DtavqenQjg`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9HttpTrans24GetResponseContentLengthEPNS0_6HandleEPbPm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `YW09CP0Vrtw`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np3ipc17ServiceIpmiClient13CreateRequestEPiiPKvm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `H35UsHYlhB4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np8SelectorD2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `PA9VYFAVKIE`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np10CancelableD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `upzdrzOYkS0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np7HttpUriD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `wSCKvDDBPy4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np15CancelableScopeD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `E8yuDNYbzl0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np10Cancelable7DestroyEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `v2yJZLY0w1U`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZThn8_N3sce2np9LocalFileD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `kD3l0P19Wzg`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZNK3sce2np10JsonString6GetStrEPcm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `ZquDaA7NrQ4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np8NpCommIdC2ERK20SceNpCommunicationId](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `WywlusFissg`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np10Cancelable10LockCancelEPKciS3_](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `fd+grYAEph0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np8WorkItem10SetPendingEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `2YbS+GhInZQ`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np7Callout4StopEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `14PDhhMEBKY`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np7RingBuf14CheckinForReadEm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `oaSKGgwTWG0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np12StreamWriter9WriteDataEPNS0_6HandleEPKvmPm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `vHaV+tsSVu4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np6ThreadD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `0Q5aKjYErBA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2npeqERK12SceNpTitleIdRKNS0_9NpTitleIdE](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `6jHOZ6fItFU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9EventFlag3SetEm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `8i-vOVRVt5w`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9RefObjectC1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `brUrttJp6MM`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZNK3sce2np7RingBuf6IsFullEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `bvJ207-8qBQ`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2npneERKNS0_13NpTitleSecretERK16SceNpTitleSecret](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `7b5y1XSa+KQ`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9LocalFileD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `-n0CR0QxhnY`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np16StreamReadBufferD2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `bH7ljyLOsBw`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np3ipc17ServiceIpmiClient11TermServiceEi](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `0rqwC4+sgzU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np6HandleD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `YWt5S4-cg9c`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np3ipc17ServiceIpmiClient17UnregisterServiceEi](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `k9jCtANC+QM`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9WorkQueue4ctorEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `XMIv42L5bEA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np6Thread4InitEPKcimm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `EqX45DhWUpo`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZNK3sce2np4Time18ConvertToPosixTimeEPl](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `qmtHTmA-CcQ`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np7Callout10IsTimedoutEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `xxOTJpEyoj4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np7Callout11CalloutFuncEPv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Zw3QlKu49eM`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZThn8_N3sce2np6HandleD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `lUsG1QfgVN4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np6Handle4InitEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `ifqJb-V1QZw`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZThn16_N3sce2np10MemoryFileD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `8S1mWU-N9kM`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_sceNpAllocatorExFree](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Zh23aSLeeZo`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np3ipc17ServiceIpmiClient7ConnectEPKvm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `NTPZ5GZIA6U`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np7RingBufC1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `L5BnZpuQImk`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np10MemoryFileD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `ZUf92uPkRuA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9HttpTrans21GetResponseStatusCodeEPNS0_6HandleEPi](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `WoFp77mNyw0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np12StreamReader8SkipDataEPNS0_6HandleElPl](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `MSMPXUL5AuM`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np13NpTitleSecretC2ERKS1_](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `JHG9CTmkdQw`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np14CalloutContextD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `S+a+rgnGX8A`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np5Mutex7DestroyEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `RgGW4f0ox1g`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np14JsonDocBuilderC1ERKNS0_9JsonValueE](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `ikFI73f3hP4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9HttpTransD2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `GSVe-aaTiEg`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np12HttpTemplateC2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `hw-UPUK9T+w`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9EventFlag6CreateEPKcj](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `amFi-Av19hU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np3ipc17ServiceIpmiClient15RegisterServiceEPKNS1_17ServiceClientInfoE](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `stUzNgtFmtY`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np8WorkItemD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Y-I66cSNp+A`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9SemaphoreD2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `OukNoRur97E`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9EventFlagD2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `RpWWfCEs9xA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9RefObjectD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `lPQzOhwPjuw`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_sceNpAllocatorMalloc](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `mzlILsFx0cU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2npneERK10SceRtcTickRKNS0_4TimeE](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `MVY+jtY-WiQ`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np13RingBufMemory6IsInitEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `zdRXyt-65kA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np7RingBufD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `hfJ1gGLgvq8`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_sceNpHeapMalloc](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `kwW5qddf+Lo`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np12HttpTemplate7DestroyEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `73qbxKjBH0o`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np15CancelableScope5BeginEPNS0_6HandleEPKciS5_](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Etvu03IpTEc`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZNK3sce2np10EventQueue6IsInitEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `VI8AHrfLdqY`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np8JsonBool5ClearEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `uDyILPgHF9Q`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9LocalFile8TruncateEl](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Sgo7wy9okFI`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZNK3sce2np10JsonNumber6GetNumEPcm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `m+dAaZ5pyO4`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np13RingBufMemory7DestroyEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Za00SEoNA2A`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9RefObject7ReleaseEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `5tYi1l9CXD0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9LocalFileC1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `QWlZu1JZOww`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np7RingBufD2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `9+NmoosRoBA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np4PathD2Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `1nF0eXrBZYM`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np12StreamReader7ReadAllEPNS0_6HandleEPvmPm](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `f1ncwa-JXlA`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np4CondD1Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `yX9ISVXv+0M`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9EventFlag7DestroyEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `QlaBcxSFPZI`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9HttpTransC1EP16SceNpAllocatorEx](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `sqNxD6H5ZOQ`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np6Thread4dtorEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `-3gV5N2u-sc`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np4Time8AddHoursEl](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Zc+a6k6i7gY`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np12WorkerThreadD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `f5L6ax7EWHk`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np13RingBufMemoryD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `70qFzq4z3UI`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np9LocalFile5CloseEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `9q+h2q5YprU`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZNK3sce2np10JsonString9GetLengthEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Wixm7ST0Syw`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np10EventQueueD0Ev](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `E4uoqSdo8ek`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np8JsonNull5ClearEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `88GKkivBFhI`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np4Time12GetUserClockEPS1_](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `vINvzJOaqws`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZN3sce2np3ipc17ServiceIpmiClient12AbortRequestEii](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `tQOrMf4KtIo`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.
- [_ZNK3sce2np5Mutex6IsInitEv](../../../core/libs/prx/libSceNpCommon/PendingExportsRound7.cpp) (NID `Yc+qj4TIEY0`) - the signature and behavior are unknown. The no-parameter declaration only resolves the import; it makes no ABI claim. Calls throw through `NotImplemented_nid_no_patch`.

### Partial virtual tables

- [Selector, buffered stream and message-pack virtual tables](../../../core/libs/prx/libSceNpCommon/InventoryExports.cpp) have the observed slot layout. Four message-pack type slots return their observed values; their other targets remain throwing declarations. The tables do not implement the underlying classes.
