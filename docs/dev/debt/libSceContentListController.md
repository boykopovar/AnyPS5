# libSceContentListController technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceContentListController/Export.cpp) - 19 entries observed in a 3.20-labelled image and a later 13.60-labelled image are import-resolution declarations only. Neither image label authenticates a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/752e1b1d4a6ac8a26c97e693527fefbd/a7bddd315c7740a7d065af9e80b0447fcd5b91be) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled module. The extract SHA-256 is `63256453dc9620091ce6af95f8b243b7da8f2ce76a585f4886f76aa6625b5b21`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. The extract is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior. The separately published 3.20 source catalog has no module file for this library.

| Name | NID |
| --- | --- |
| `sceCLCClearFileList` | `nAF1xF5QRHk` |
| `sceCLCClearTrackList` | `yx2QM7H-nww` |
| `sceCLCGetDeviceInfo` | `dYe+SpaG3vI` |
| `sceCLCGetDeviceList` | `Aqs-JQ9uJUc` |
| `sceCLCGetFileInfo` | `cq8s1ijWULA` |
| `sceCLCGetFileList` | `GvFZOeEJ9aw` |
| `sceCLCGetFileListInfo` | `tulcX6FKfk0` |
| `sceCLCGetFolderInfo` | `lm4JK2NTzoA` |
| `sceCLCGetShuffle` | `2Vf60vmDN3A` |
| `sceCLCGetTrackInfo` | `ZidmCssGGUs` |
| `sceCLCGetTrackList` | `HECulNjRDBA` |
| `sceCLCGetTrackListInfo` | `H1lNYyrbrlQ` |
| `sceCLCGetUsbStorageCallback` | `DGHwTmra1QE` |
| `sceCLCRegisterEventCallback` | `ujyPLnX7SlE` |
| `sceCLCSetPlaybackResult` | `GC51I89ii44` |
| `sceCLCSetShuffle` | `EPOYwPzb14s` |
| `sceCLCSetTrackList` | `5nwdtz34v7I` |
| `sceCLCSetUsbStorageCallback` | `037hAMY7tHY` |
| `Unknown` | `SoBk-kuNLsE` |
