# libReactNative.Modules.Vsh technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libReactNative.Modules.Vsh/Export.cpp) - 65 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libReactNative.Modules.Vsh.c) and the [selected export metadata](https://gist.github.com/kyon-eth/c09d6fdb71ceddd6d272f23ccc02ea4d/c875018fc77e54dcf76c639ceb7a58b0a85cf16b) from two version-labelled images (SHA-256 `ee343a3ad0c3badb1ed2ebff777bcc24cc00fbfedf7797ae37ed51b4d46dcd9e`). The labels are not authenticated. Prototypes and behavior are unverified; the declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality. Data exports are not included. Three other functions shared with the WebAppLauncher library need separate provider-resolution analysis.

| Name | NID |
| --- | --- |
| `CreateWebApiLibCtx` | `MjYzx34cnmE` |
| `CryptographyDecryptAES128CBC` | `x4fETeuAcVU` |
| `CryptographyDecryptAES128CBC_HS256` | `1HSkObnLOC0` |
| `CryptographyDecryptAES128GCM` | `MaAqY5ufzmk` |
| `CryptographyEncryptAES128CBC` | `WM4KfSxNA3I` |
| `CryptographyEncryptAES128GCM` | `SYc9wxeZia8` |
| `CryptographyGenerateHmacSha256` | `0L0rPrqMqqE` |
| `DestroyWebApiLibCtx` | `RJ9NntjPcEs` |
| `GetLibHttpCtxId` | `cCMDYO9tVWc` |
| `GetPs4AppCategoryForTitleId` | `bmzRityxR44` |
| `InetStatGetResultNative` | `nNDGebQ9xr0` |
| `InetStatGetStateNative` | `katLYKl5Xhw` |
| `InetStatStartTestNative` | `iTT38xYbOqI` |
| `InitializeNetLib` | `YzB8-Aa5UV4` |
| `LoginMgrCreateUser` | `jpb8rbo4MwU` |
| `LoginMgrDestroyUser` | `-N9SbnOwNto` |
| `LoginMgrInitialize` | `lvsjPQ8LMH0` |
| `LoginMgrSetLoginFlag` | `xXkwDDcyfBw` |
| `LoginMgrSetUserStatus` | `cxRN3ES1Hmk` |
| `LoginMgrTerminate` | `CBgGpKwrHSA` |
| `NpStateManagerRegisterCallback` | `D3CRTfQdhbA` |
| `NpStateManagerRtcSetTick` | `P6fVk0LPubc` |
| `NpStateManagerStart` | `eXL2pNf3JSA` |
| `NpStateManagerStop` | `5gsmp2Ox0Vs` |
| `NpStateManagerUnregisterCallback` | `f5Zum3vudFM` |
| `PSNowGetStatus` | `BFyDpKjORBg` |
| `PSNowGetStreamingStatus` | `ln8IwqIWE4I` |
| `PSNowInitialize` | `9Gsw6lvdddE` |
| `PSNowReset` | `gm2mcS8itac` |
| `PSNowSendCommand` | `mjcI9VABYqw` |
| `PSNowSetAuthCodes` | `PMAYiRpjoMY` |
| `PSNowSetSettings` | `H0yW7HkbWzI` |
| `PSNowTerminate` | `FbdEMaWO06g` |
| `RemotePlayClearConnectHistory` | `1nU-9usW+G0` |
| `RemotePlayConfirmDeviceRegist` | `Sh2l-tqTK68` |
| `RemotePlayDisconnect` | `Tjr5ksLUG1o` |
| `RemotePlayGeneratePinCode` | `Ktw+4DDd-60` |
| `RemotePlayGetConnectHistory` | `Pp9o-vFsZxc` |
| `RemotePlayGetConnectUserId` | `sB3dPDza3bk` |
| `RemotePlayGetConnectionStatus` | `PIt3ROlWOic` |
| `RemotePlayGetRemoteplayStatus` | `Mio-My1B0Ek` |
| `RemotePlayGetRpMode` | `4U7XQYIZAI0` |
| `RemotePlayIsRemotePlaying` | `RXRWD6iVzvI` |
| `RemotePlayNotifyCanWakeup` | `p1zwikCkFho` |
| `RemotePlayNotifyPinCodeError` | `xiJubl4cUak` |
| `RemotePlayNotifyUserDelete` | `GyKvp1lgCFw` |
| `RemotePlayProhibitStreaming` | `JSbNmt85AcQ` |
| `RemotePlaySetProhibition` | `BOtuzpAvdiU` |
| `RemotePlaySetProhibitionForVsh` | `4T9kRhpb-V0` |
| `RemotePlaySetRpMode` | `2YfXXadue0s` |
| `RtcGetCurrentAdNetworkTickNative` | `BMqsRTJXTRk` |
| `RtcGetCurrentDebugNetworkTickNative` | `8WzUhuCvVx4` |
| `RtcGetCurrentNetworkTickNative` | `m7od1ikoZpQ` |
| `TerminateNetLib` | `n38wJyalO6c` |
| `WebBrowserGetEnvelopeData` | `Z5HNtKvubuo` |
| `sceRemotePlayClientAddEventListener` | `gezFjY7iHbk` |
| `sceRemotePlayClientAttachVirtualMicrophone` | `nQ9t43vahAE` |
| `sceRemotePlayClientDetachVirtualMicrophone` | `nePk3oFOMXA` |
| `sceRemotePlayClientGetClientStatus` | `ux9VESJUgh4` |
| `sceRemotePlayClientInitialize` | `R0TfhbZMHm4` |
| `sceRemotePlayClientRemoveController` | `tDjU6UN7meM` |
| `sceRemotePlayClientShowHomeScreen` | `T589M5rT9iw` |
| `sceRemotePlayClientStartMicCapture` | `r9MlfKcu6N4` |
| `sceRemotePlayClientStopMicCapture` | `jLpO843jV50` |
| `sceRemotePlayClientTerminateStreaming` | `N1dFmppU9j0` |

### Additional unidentified exports

- Eighteen [callable exports](../../../core/libs/prx/libReactNative.Modules.Vsh/Export.cpp) are throwing placeholders. Names, signatures and behavior are unverified. Matching module and library identities occur in two version-labelled images; those labels are not authenticated.

| Name | NID |
| --- | --- |
| `Unknown` | `+I5sPs9FLXw` |
| `Unknown` | `-9hYGDAsb4E` |
| `Unknown` | `BzJsgzD97LU` |
| `Unknown` | `IQNf2nnftNI` |
| `Unknown` | `N3mcm-qDFR0` |
| `Unknown` | `OME2Be+FlwA` |
| `Unknown` | `PLEe-toB-KE` |
| `Unknown` | `QGGsQrtLW9Y` |
| `Unknown` | `Rm+rgnBoPIE` |
| `Unknown` | `SJU+qK+8yrM` |
| `Unknown` | `T7dL-hEySvU` |
| `Unknown` | `b+MJDz3HB6s` |
| `Unknown` | `bbsP8-e8GZA` |
| `Unknown` | `fxEQAPDRhLw` |
| `Unknown` | `j917Pp4k-CI` |
| `Unknown` | `lZ-JDEqB34k` |
| `Unknown` | `tKy-Qdg+FRc` |
| `Unknown` | `uQfQgwm36L4` |
