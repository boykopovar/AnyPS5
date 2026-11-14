# libNativeExtensions technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libNativeExtensions/Export.cpp) - 31 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libNativeExtensions.c) and the [selected export metadata](https://gist.github.com/kyon-eth/ec9d983229251f3a5a4c1507f93212ac/94e6eb205a86dca0688405d05b10f2fef3e60fcd) from two version-labelled images (SHA-256 `a52ab8521e9e39bc899d95f6b85a5284d3fe3ed2688f943fe2a77a65bb2f4796`). The labels are not authenticated. Prototypes and behavior are unverified; the declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality. Data exports in the later image are not included.

| Name | NID |
| --- | --- |
| `AsyncStorageClearNative` | `3eCvb-utsDw` |
| `AsyncStorageCloseNative` | `jiuuJN7Ux0Q` |
| `AsyncStorageGetAllKeysNative` | `oirBv8VDfaA` |
| `AsyncStorageGetItemNative` | `lKfctLchxNs` |
| `AsyncStorageOpenNative` | `+YbLnJwL+xw` |
| `AsyncStorageRemoveItemNative` | `IjDmpUrMSz0` |
| `AsyncStorageSetItemNative` | `xYf-z05L7UE` |
| `BgsStorageClose` | `sBHhzqDUvkU` |
| `BgsStorageFreeKeyValueResult` | `g9E2ShyXqJU` |
| `BgsStorageFreeListResult` | `jkw52aHWgtk` |
| `BgsStorageGetContainers` | `nkbTjX8ghpY` |
| `BgsStorageGetItems` | `nEusu9xBLh0` |
| `BgsStorageGetItemsBySearch` | `B0PtaDqaYm0` |
| `BgsStorageOpen` | `kevKTD6kQxw` |
| `CurlGetDefaultUA` | `mtAGcMQCufk` |
| `CurlMultiInitialize` | `xjVZ2aqjVrg` |
| `CurlMultiQueueRequest` | `XuNOIRPz7Os` |
| `DecryptRnpsBundle` | `hP81Fkk27pg` |
| `HttpCacheWrapperFree` | `ifOC0Tz3yMQ` |
| `HttpCacheWrapperRetrieve` | `h+TdWvCbo-Q` |
| `HttpCacheWrapperRevalidate` | `Zi4XKSpB7ss` |
| `NetCtlCheckCallbackNative` | `8I7xekkfFhQ` |
| `NetCtlGetDeviceTypeNative` | `z4zamLphAP4` |
| `NetCtlGetStateNative` | `UBtYSUByi3M` |
| `NetCtlInitNative` | `orTn+qafMUs` |
| `NetCtlRegisterCallbackNative` | `rlfaE8TqUJI` |
| `NetCtlTermNative` | `SKMDsFP5-Cs` |
| `NetCtlUnregisterCallbackNative` | `6lwSxBExYLo` |
| `RnpsGetOpenPsIdForSystem` | `85O5VmSVqmI` |
| `RnpsGetTicks` | `dG6FGUzIYGA` |
| `ThreadUtils_SetSchedulePolicy` | `KGAA02fSVmk` |

### Additional unidentified exports

- Six [callable exports](../../../core/libs/prx/libNativeExtensions/Export.cpp) are throwing placeholders. Names, signatures and behavior are unverified. Matching module and library identities occur in two version-labelled images; those labels are not authenticated.

| Name | NID |
| --- | --- |
| `Unknown` | `6QMoWxdywac` |
| `Unknown` | `D-2SZbPFqzQ` |
| `Unknown` | `J4I3Ue9vQ+E` |
| `Unknown` | `PYd486Ryp+4` |
| `Unknown` | `QfxvUZ0N9zc` |
| `Unknown` | `tw7mDSn+8Ng` |

- [Older callable NIDs](../../../core/libs/prx/libNativeExtensions/Export.cpp): 11 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.
