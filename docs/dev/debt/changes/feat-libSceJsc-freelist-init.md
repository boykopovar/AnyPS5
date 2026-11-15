# libSceJsc

## add Functional

- [JSC::FreeList initializeList and initializeBump](../../../core/libs/prx/libSceJsc/src/FreeList.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version and the 40-byte layout are unverified. They follow pinned [FreeList.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/heap/FreeList.cpp#L49-L66) on the members of [FreeList.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/heap/FreeList.h#L99-L104): the first 32 bytes are written through `GuestCopyOut_nid_no_patch` and the cell size at 32 is kept. A null or unwritable object throws where WebKit faults. PS5 11.x [FreeList.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/heap/FreeList.h) has neither name. Closes when: compared with a console module.

## remove | `_ZN3JSC8FreeList14initializeBumpEPcj` | `4FTNJKyJYaE` |

## remove | `_ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj` | `Q2lC2jg+CZk` |

## change [Additional callable exports](../../../core/libs/prx/libSceJsc/Export.cpp)

- [Additional callable exports](../../../core/libs/prx/libSceJsc/Export.cpp) - 309 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceJsc.c) and the [selected export metadata](https://gist.github.com/kyon-eth/e699987524d8485a9de9740280cfe4c1/09c89f4d64a17da12485e0eb637620ced4f6eeb2) from two version-labelled images (SHA-256 `d2a5f8a8807f23cb71170a5d3b779a57636f5a3480d92955e4255545ffe1e97e`). The labels are not authenticated. Apart from the 13 register-set queries and constructors and the two `Deprecated::ScriptValue` getters in the firmware 13.42 entries below, prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality. Shared WebKit exports require separate loader analysis.
