# libSceAbstractYoutube technical debt

### Known export names; ABI prototypes unverified

- 17 pending exports in [Export.cpp](../../../core/libs/prx/libSceAbstractYoutube/Export.cpp) are present in the [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs) inventory. Their prototypes and behavior are unverified. They use the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAbstractYoutube/Export.cpp) - 39 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `_ZN15AbstractStorage13YoutubeFolder6RemoveEb` | `sFDmBXjzJKs` |
| `_ZN15AbstractStorage13YoutubeFolderD0Ev` | `oFx2mmWanvg` |
| `_ZN15AbstractStorage13YoutubeFolderD1Ev` | `lkGvBL1ly-Y` |
| `_ZN15AbstractStorage13YoutubeFolderD2Ev` | `KdoemZ8qogY` |
| `_ZN15AbstractStorage14YoutubeContent11SetMetadataEPN23sceMetadataReaderWriter8MetadataE` | `835p-ywJWEY` |
| `_ZN15AbstractStorage14YoutubeContent13ParseMetadataEPN23sceMetadataReaderWriter8MetadataES3_` | `iYDibzjdBf8` |
| `_ZN15AbstractStorage14YoutubeContent4OpenEit` | `gID4V0+Uk+k` |
| `_ZN15AbstractStorage14YoutubeContent4ReadEPvlPl` | `MJP2vvOKllQ` |
| `_ZN15AbstractStorage14YoutubeContent5CloseEv` | `BbkdawQaHyg` |
| `_ZN15AbstractStorage14YoutubeContent5FlushEv` | `Tz3OEt87piQ` |
| `_ZN15AbstractStorage14YoutubeContent5WriteEPKvlPl` | `95+sJrPf30A` |
| `_ZN15AbstractStorage14YoutubeContent6RemoveEv` | `XSYZ7Ac3HBQ` |
| `_ZN15AbstractStorage14YoutubeContent8SeekByteEliPl` | `d5LwAYdjVqk` |
| `_ZN15AbstractStorage14YoutubeContent8SeekTimeElPl` | `uHAsYIPFXPA` |
| `_ZN15AbstractStorage14YoutubeContentD0Ev` | `3XwULRPa25E` |
| `_ZN15AbstractStorage14YoutubeContentD1Ev` | `TFyVtXqnZrA` |
| `_ZN15AbstractStorage14YoutubeContentD2Ev` | `F09B23u5GSU` |
| `_ZN15AbstractStorage14YoutubeService13SearchStorageEv` | `6OnmSnvjzPI` |
| `_ZN15AbstractStorage14YoutubeService4StopEv` | `7JwKchX2P8o` |
| `_ZN15AbstractStorage14YoutubeService5StartEv` | `DGbb+VBzV6c` |
| `_ZN15AbstractStorage14YoutubeServiceC1Ev` | `UyVVO5GxZl4` |
| `_ZN15AbstractStorage14YoutubeServiceC2Ev` | `sYyou0hGbP8` |
| `_ZN15AbstractStorage14YoutubeServiceD0Ev` | `J526sLKBCc8` |
| `_ZN15AbstractStorage14YoutubeServiceD1Ev` | `jGcnzPEHoZ8` |
| `_ZN15AbstractStorage14YoutubeServiceD2Ev` | `dhk0U1yIuJo` |
| `_ZN15AbstractStorage14YoutubeStorage14GetStorageSizeEPlS1_S1_` | `u6FSGzt9Ay0` |
| `_ZN15AbstractStorage14YoutubeStorageD0Ev` | `VkzubmxnY7s` |
| `_ZN15AbstractStorage14YoutubeStorageD1Ev` | `D5iaQceaLcI` |
| `_ZN15AbstractStorage14YoutubeStorageD2Ev` | `hfifwAJDCvU` |
| `_ZNK15AbstractStorage13YoutubeFolder10GetServiceEv` | `cyheZerH-jM` |
| `_ZNK15AbstractStorage13YoutubeFolder13GetCapabilityEv` | `2jgqoWLtP4o` |
| `_ZNK15AbstractStorage13YoutubeFolder7GetStatEPNS_4StatE` | `Frm1N1BC9XE` |
| `_ZNK15AbstractStorage14YoutubeContent10GetServiceEv` | `zIMH5hIuWk4` |
| `_ZNK15AbstractStorage14YoutubeContent13GetCapabilityEv` | `EDw-LQKe3fE` |
| `_ZNK15AbstractStorage14YoutubeContent7GetStatEPNS_4StatE` | `+SMBrTixsiA` |
| `_ZNK15AbstractStorage14YoutubeService14GetServiceTypeEv` | `UTsKij05zBY` |
| `_ZNK15AbstractStorage14YoutubeStorage10GetServiceEv` | `uVLETIxb0yA` |
| `_ZNK15AbstractStorage14YoutubeStorage13GetCapabilityEv` | `jgO+nc-VCgU` |
| `_ZNK15AbstractStorage14YoutubeStorage7GetStatEPNS_4StatE` | `GGthfnlokVw` |
