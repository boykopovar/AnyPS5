# libReactNative.Modules.Vsh.MediaPlayer.Contents technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libReactNative.Modules.Vsh.MediaPlayer.Contents/Export.cpp) - 5 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libReactNative.Modules.Vsh.MediaPlayer.Contents.c) and the [selected export metadata](https://gist.github.com/kyon-eth/af824574a9e552c751d9cb64c18ff212/440b12f2feb3adb1eb8bee7050758abcb7606a47) from two version-labelled images (SHA-256 `04df41b0355a12377766d004b73da51fab498b2995375bb4d0dbc90e94058ff6`). The labels are not authenticated. Prototypes and behavior are unverified; the declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| `GetContentFileStatusLib` | `4Yyat0vtan0` |
| `GetPhotoInfoLib` | `7niGF7s7qHU` |
| `GetUsbThumbnailEntryListLib` | `1d7jXAh8SH8` |
| `InitializeLib` | `SPVkdqBsosQ` |
| `TerminateLib` | `QMHNcV-DHGE` |

### Additional unidentified exports

- Two [callable exports](../../../core/libs/prx/libReactNative.Modules.Vsh.MediaPlayer.Contents/Export.cpp) are throwing placeholders. Names, signatures and behavior are unverified. Matching module and library identities occur in two version-labelled images; those labels are not authenticated.

| Name | NID |
| --- | --- |
| `Unknown` | `+9Jlq11BFrI` |
| `Unknown` | `hRs5qGt0+5Q` |
