# libReactNative.Modules.Vsh.WebAppLauncher technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libReactNative.Modules.Vsh.WebAppLauncher/Export.cpp) - 45 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libReactNative.Modules.Vsh.WebAppLauncher.c) and the [selected export metadata](https://gist.github.com/kyon-eth/b36a766ab0a33d469711188e09f19e2a/cc181f10ffa02358b56e235c69f5af19cfaf6d89) from two version-labelled images (SHA-256 `14fb80674fe2054933885dc3fa8a8abaad406191cdae915eb1a91b5580cae0a8`). The labels are not authenticated. Prototypes and behavior are unverified; the declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality. Three other functions shared with the React Native Vsh library need separate provider-resolution analysis.

| Name | NID |
| --- | --- |
| `CheckSystemEvents` | `VC8PCwSx3g0` |
| `CommerceDialogBrowseCategory` | `yNFPPeNhIwk` |
| `CommerceDialogBrowseProduct` | `YpeJJ7DL1Zo` |
| `CommerceDialogInit` | `o3tYKx8oBBA` |
| `CommerceDialogRedeemPromotionCode` | `ZLINx2XbQQ8` |
| `CommerceHidePsStoreIcon` | `k7y31RwRcGg` |
| `CommerceSetPsStoreIconLayout` | `3rXpEBCRi1o` |
| `CommerceShowPsStoreIcon` | `9-iVwdotSr4` |
| `DisableSystemMedia` | `jlIfgOEzZ2A` |
| `ErrorDialogClose` | `PUnaOnE5qv4` |
| `ErrorDialogInit` | `WeZCXuG60OQ` |
| `ErrorDialogOpen` | `ds9BJbeJ0ZQ` |
| `GetAccountAgeA` | `mypq-tQJOlo` |
| `GetAccountCountryA` | `+UmoBB-Kur0` |
| `GetAccountIdA` | `W17CX8bEsI4` |
| `GetAccountLanguage2` | `AitA5hf0b4o` |
| `GetAuthorizationCode` | `zn1GN6GVKwY` |
| `GetClosedCaptionSettings` | `xVlRy5NAUsw` |
| `GetOnlineId` | `2uw51wxVsiI` |
| `GetProcessTime` | `FQz66HusRxA` |
| `GetVideoOutputStatus` | `19LEprfkGS0` |
| `GetVideoOutputStatusInt` | `ndc59UeFwpE` |
| `Hide` | `6dhVu-SHjHI` |
| `IsDebuggingSupported` | `6eoxFFSEuoY` |
| `NpWebAbortRequest` | `eNQ0FStvU1c` |
| `NpWebDeleteRequest` | `yjjDwtgu4jE` |
| `NpWebInitialize` | `0zuAEore8pY` |
| `NpWebReadData` | `9gfFGr6B1Fc` |
| `NpWebTerminate` | `sXn8uN-ijvw` |
| `OpenClosedCaptionAdvancedSettings` | `fqERqkvThgQ` |
| `OpenClosedCaptionSettings` | `kroxOU831V4` |
| `SoundInitialize` | `82UnAk1CC98` |
| `SoundLoad` | `7x711wfwKEM` |
| `SoundPlay` | `k77hI5zSW1c` |
| `SoundTerminate` | `pO9cpXuuPwU` |
| `SpeechCancel` | `rwnTeqYe0bY` |
| `SpeechClose` | `7SmqB-eXB+4` |
| `SpeechInitialize` | `9ozXeR1-w4c` |
| `SpeechSpeak` | `DceITXrEqMI` |
| `SpeechStatus` | `cJ1aLzpbyow` |
| `SpeechTerminate` | `W5pdIiQSrmY` |
| `SystemDialogInit` | `zmUqZyLpEhs` |
| `SystemDialogOpen` | `AVyuvygnd0w` |
| `SystemServiceParamGetInt` | `Ewd+kXqU2Bc` |
| `SystemServiceParamGetString` | `vqMvoe76W6k` |

### Additional unidentified exports

- Twelve [callable exports](../../../core/libs/prx/libReactNative.Modules.Vsh.WebAppLauncher/Export.cpp) are throwing placeholders. 11 are named after the symbols the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NIDs, and the NID of each name matches; the other is unidentified. Signatures and behavior are unverified. Matching module and library identities occur in two version-labelled images; those labels are not authenticated.

| Name | NID |
| --- | --- |
| `EnableSystemMusic` | `+ucrAt9fI7c` |
| `SpeechSystemSettings` | `06N-dAyE+HY` |
| `Unknown` | `41blU2JCqLQ` |
| `GetAccountPsPlusStatus` | `BGgSEXB1Now` |
| `RegisterSystemEventCallback` | `CAK-S2JVdsk` |
| `GetNetworkConnectionType` | `QxCdpkEsOgo` |
| `NpWebSendRequest` | `Tjj++S2qvOU` |
| `DisableSystemMusic` | `f5NHqUziG+4` |
| `EnableSystemMedia` | `mI-ltMuVN+Q` |
| `CommerceDialogCheckout` | `vDup+0mWZf8` |
| `CommerceDialogBrowsePsPlus` | `wR1ucKGDM9Q` |
| `NpWebCreateRequest` | `xPA-tXzPGpc` |

### Additional 13.60 catalog exports

- [13.60 catalog exports](../../../core/libs/prx/libReactNative.Modules.Vsh.WebAppLauncher/Export.cpp) - 78 further named callable exports are `defined_function_export` records with `name_status: verified_nid_binding`, `signature_status: unknown` and a confirmed 13.60 export in the [PS5 13.60 function stub library catalog](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/ba1933f305f1934ed2a12c94479a977844bdbe5a/data/firmware-13.60/libreactnative-modules-vsh-webapplauncher-46dbce980c.jsonl) and were absent from the repository; each name additionally reproduces its catalog NID under `tools/nid_names.py`. Prototypes and behaviour are unverified: the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`. Console behaviour is not measured.

| Name | NID |
| --- | --- |
| `AdIdDialogOpen` | `7QaCS1UMWwI` |
| `AuthorizedAppDialogOpen` | `G9vv+l6S2Mo` |
| `ConnectAccountDialogInit` | `Ur+UIP9Ksu0` |
| `ConnectAccountDialogOpen` | `wpnXKCdeZMs` |
| `GetAccessibilitySettings` | `RdxTTpo-rGQ` |
| `GetAppSessionId` | `YweBmH1BuXs` |
| `GetAuthorizedAppCode` | `9FhGU9KHB9o` |
| `GetClosedCaptionAllSettings` | `JdlJ+eMK1l8` |
| `GetFileSize` | `N-mLEg1EsU8` |
| `GetMessage` | `eoSSfTyJ0pU` |
| `GetNpEnvironment` | `8+XvcUKEnVM` |
| `GetOpenPsIdForSystem` | `8oA9yaJwLRs` |
| `GetUniversalEsvmDebugSetting` | `NDbiML-3VQ0` |
| `IsSupportedAudioOutputMode` | `EpysYXPQcbc` |
| `IsSupportedVideoOutputMode` | `NaV6CitH+6w` |
| `MusicCoreAbortMessage` | `oHYy1meDdfQ` |
| `MusicCoreAddSrc` | `rOxw9r23WnI` |
| `MusicCoreGetCapability` | `iGZ0ggtDTFo` |
| `MusicCoreGetCurrentTime` | `KftprRVmnv0` |
| `MusicCoreGetDuration` | `ucROIWQejbs` |
| `MusicCoreGetLoop` | `Abuz+2VZmD4` |
| `MusicCoreGetMusicInfo` | `TLWM+JaZoKE` |
| `MusicCoreGetMuted` | `KG05tU-FFyA` |
| `MusicCoreGetNetworkState` | `X5Y6rIhQ4eQ` |
| `MusicCoreGetShuffle` | `--cudfTMQfc` |
| `MusicCoreGetVolume` | `E-mvO8cPQQU` |
| `MusicCoreInitialize` | `aAqsuB33+04` |
| `MusicCoreIsEnded` | `IcwkCcjP9W8` |
| `MusicCoreIsPaused` | `4ViuChpinTg` |
| `MusicCoreIsPlaying` | `8hJY7ny2quA` |
| `MusicCoreIsSeeking` | `GuOrK5jujoo` |
| `MusicCoreIsStopped` | `c+HIxuLmhQw` |
| `MusicCoreLoad` | `HXINhVhBEko` |
| `MusicCorePause` | `rxJHQBCKWtc` |
| `MusicCorePlay` | `vFPhWyX7UR4` |
| `MusicCoreRegisterCallback` | `GC+osxDYIP8` |
| `MusicCoreRegisterNotify` | `1T8k4JZX6Hw` |
| `MusicCoreRemoveSrc` | `LH+MNa9g7XM` |
| `MusicCoreSendMessageAsync` | `FNogYBEfz-E` |
| `MusicCoreSendMessageSync` | `7mDHTWWTQ3o` |
| `MusicCoreSetCurrentTime` | `OzQQPEaoQEQ` |
| `MusicCoreSetLoop` | `571FHdsZucE` |
| `MusicCoreSetMuted` | `5YfrNlm+v6U` |
| `MusicCoreSetShuffle` | `Wr-BSjpuG-M` |
| `MusicCoreSetVolume` | `UiA6ELrGuIM` |
| `MusicCoreSkipNext` | `xJli9ZxRCh8` |
| `MusicCoreSkipPrevious` | `LfnmOy3UTZQ` |
| `MusicCoreStop` | `07VBNMZQovE` |
| `MusicCoreTerminate` | `-IldEhNuAvM` |
| `MusicCoreUnregisterCallback` | `gHoAka+O548` |
| `MusicCoreUnregisterNotify` | `t15nfpTmCKU` |
| `NpWebGetPushContextId` | `Z8hcRdW0kDE` |
| `NpWebSetAdditionalScope` | `2erd+bVjKSU` |
| `OpenApp` | `ZUbGNhlb0wY` |
| `OpenVRApp` | `DUHeFECzUaA` |
| `PartnerModuleInitialize` | `WD+Kt5fP+NA` |
| `PartnerModuleTerminate` | `s0UsPmb2uy0` |
| `PostMessage` | `0BebaW7IIwc` |
| `ReadFile` | `rM01bNYVd60` |
| `SaveDataCommit` | `1r+UZaDGVpQ` |
| `SaveDataDelete` | `EhZpnWkXeuE` |
| `SaveDataInitialize` | `Ej8MMaxAknc` |
| `SaveDataMount` | `+WhO08ZXOVs` |
| `SaveDataPrepare` | `4pcLNpZ1Jxg` |
| `SaveDataReadFile` | `TBaYryopgpA` |
| `SaveDataReadFileSize` | `dmfQGvigL8w` |
| `SaveDataTerminate` | `s4jtKu6tSaA` |
| `SaveDataUnmount` | `0i+TAb24YYg` |
| `SaveDataWriteFile` | `m9EZh34aNrQ` |
| `SendNativeTelemetryCrashEvent` | `LhR2nQ-Zfqc` |
| `SetAudioTagFeature` | `Cnt120u0Kco` |
| `SetUniversalEsvm` | `NxnLFoYWqkM` |
| `SetVideoOutputMode` | `JQHnPL1OnMM` |
| `ShareFeaturePermit` | `wTMarn0GErU` |
| `ShareFeatureProhibit` | `4Lx3K-A682o` |
| `ShareGetCurrentStatus` | `EZb6o3iQ36c` |
| `ShareInitialize` | `+GG9O5nkoLg` |
| `ShareTerminate` | `9PxZzPbFSAo` |
