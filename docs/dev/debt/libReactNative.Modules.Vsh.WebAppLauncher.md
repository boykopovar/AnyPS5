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

- Twelve [callable exports](../../../core/libs/prx/libReactNative.Modules.Vsh.WebAppLauncher/Export.cpp) are throwing placeholders. Names, signatures and behavior are unverified. Matching module and library identities occur in two version-labelled images; those labels are not authenticated.

| Name | NID |
| --- | --- |
| `Unknown` | `+ucrAt9fI7c` |
| `Unknown` | `06N-dAyE+HY` |
| `Unknown` | `41blU2JCqLQ` |
| `Unknown` | `BGgSEXB1Now` |
| `Unknown` | `CAK-S2JVdsk` |
| `Unknown` | `QxCdpkEsOgo` |
| `Unknown` | `Tjj++S2qvOU` |
| `Unknown` | `f5NHqUziG+4` |
| `Unknown` | `mI-ltMuVN+Q` |
| `Unknown` | `vDup+0mWZf8` |
| `Unknown` | `wR1ucKGDM9Q` |
| `Unknown` | `xPA-tXzPGpc` |
