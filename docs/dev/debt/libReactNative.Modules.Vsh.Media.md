# libReactNative.Modules.Vsh.Media technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libReactNative.Modules.Vsh.Media/Export.cpp) - 39 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libReactNative.Modules.Vsh.Media.c) and the [selected export metadata](https://gist.github.com/kyon-eth/a4287d22e315cdf884e1c90e5818bc96/bc5d6fd53561679b5940a0abf2eac90317076d11) from two version-labelled images (SHA-256 `bea71d8716c29f4f4a92cd3d9eaf797681ad42e10cef3d96471ca5f0f7be0889`). The labels are not authenticated. Prototypes and behavior are unverified; the declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| `FinalizeMediaModule` | `EbfE0StszWE` |
| `ForbidCopyrightProtectedContents` | `bwYSgo0Bkro` |
| `GetCapabilities` | `RLXAhRHJT04` |
| `GetCurrentSrcId` | `tAoG7GBkUZ4` |
| `GetCurrentTime` | `5rDyTRHHx3k` |
| `GetMediaPlaybackInfo` | `H8ufT+mZDyM` |
| `GetMute` | `wIeVYYUX2ak` |
| `GetServiceType` | `glBkV8-mprA` |
| `GetServiceTypeFromTitleId` | `oZLnhi8kH8Y` |
| `GetSrcIdList` | `Uf6y9t5GHeQ` |
| `GetUsbDeviceListArray` | `OucBlCU37BI` |
| `GetVolume` | `udoSfHla8yA` |
| `InitializeMediaModule` | `JwZ32l8Yxx0` |
| `IsSrcIdPlaying` | `7mUmOtv4Etc` |
| `MASSMADeleteMessage` | `jiJJmEiTth8` |
| `MASSMAInitialize` | `qSPtYrJTJMo` |
| `MASSMASendMessage` | `U2kjiIEZalE` |
| `MASSMATerminate` | `2x55BYiVzCc` |
| `MASSMCDeleteMessage` | `gRVX2CeO2WA` |
| `MASSMCReadMessage` | `xxCPbEBdoEU` |
| `MASSMCTerminate` | `Q2alI2Oi1wE` |
| `MASSValueSizeMax` | `d2UmeNMcWZI` |
| `PlayUri` | `wbFpBDuG7d8` |
| `PlaybackCommand` | `CtK8g2qdXwI` |
| `RemoveMediaSource` | `ADN8G76X6xg` |
| `Seek` | `K6JphzOV+Bc` |
| `ServiceSupported` | `uzm8HX1wuCY` |
| `SetCurrentSrcId` | `X74NRPVnffM` |
| `SetCurrentTime` | `ZEF2Njwr8Kk` |
| `SetMute` | `8Unr2gwsENc` |
| `SetServiceType` | `GXcCQB-7-Sc` |
| `SetVolume` | `nP-5Rnjgj10` |
| `addProhibitionFlagListener` | `vr6DMbEWSWM` |
| `addTelemetryErrorListener` | `rQht6vu1qCc` |
| `getDecryptionKeys` | `sv9yvbNVpAs` |
| `getProhibitionFlags` | `ENgBTg9H2yI` |
| `removeProhibitionFlagListener` | `7oRxmHRqfCY` |
| `removeTelemetryErrorListener` | `JZMm1O3sBAU` |
| `setVideoMode` | `po1M8887cuw` |

### Additional unidentified exports

- Nine [callable exports](../../../core/libs/prx/libReactNative.Modules.Vsh.Media/Export.cpp) are throwing placeholders. Names, signatures and behavior are unverified. Matching module and library identities occur in two version-labelled images; those labels are not authenticated.

| Name | NID |
| --- | --- |
| `Unknown` | `BIdvn6c-zAw` |
| `Unknown` | `BwF-lBIdW5g` |
| `Unknown` | `JwW+jnYDAcI` |
| `Unknown` | `QWwXJ-KSWg4` |
| `Unknown` | `S3lY-8Af+rw` |
| `Unknown` | `Xn1h2KUpUN4` |
| `Unknown` | `kYVOipktdO0` |
| `Unknown` | `m4NzcIr-JJk` |
| `Unknown` | `zj7-XSRULNw` |
