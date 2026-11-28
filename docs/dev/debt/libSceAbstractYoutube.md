# libSceAbstractYoutube technical debt

### Known export names; ABI prototypes unverified

- 17 pending exports in [Export.cpp](../../../core/libs/prx/libSceAbstractYoutube/Export.cpp) are present in the [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs) inventory. Their prototypes and behavior are unverified. They use the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.

- [Immediate returns](../../../core/libs/prx/libSceAbstractYoutube/Export.cpp) (libSceAbstractYoutube) - the return types of 18 exports and what `0x809E0005` means are unverified; the parameters come from the mangled names. In the 13.40 module each body returns at once without reading an argument: the `YoutubeContent`, `YoutubeFolder` and `YoutubeStorage` `GetStat`, `YoutubeContent` `Read`, `SeekTime`, `Flush` and `Remove`, `YoutubeFolder::Remove`, `Open` and `Close`, and `YoutubeStorage::GetStorageSize` return `0x809E0005`, the three `GetService` and `YoutubeService::GetServiceType` 2, and `GetCapability` `0x6C0A` for `YoutubeContent`, `0x1000000000` for `YoutubeFolder` and `0x180000000000000` for `YoutubeStorage`, the last two in the full 64-bit return register. Read from disassembly, not measured on a console.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAbstractYoutube/Export.cpp) - 23 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAbstractYoutube.c) and the [selected export metadata](https://gist.github.com/kyon-eth/d35bcd082a172a28ada2052647b0ff20/72a4f16fcb238e61dc8c6424dc26eee106e6c8ce) from two version-labelled images (SHA-256 `3d83607a2ed81110a37d677e5ba1936a01cb78c5847a2e9b582a4a981d313050`). The labels are not authenticated. Prototypes and behavior are unverified; the declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| `_ZN15AbstractStorage13YoutubeFolderD0Ev` | `oFx2mmWanvg` |
| `_ZN15AbstractStorage13YoutubeFolderD1Ev` | `lkGvBL1ly-Y` |
| `_ZN15AbstractStorage13YoutubeFolderD2Ev` | `KdoemZ8qogY` |
| `_ZN15AbstractStorage14YoutubeContent11SetMetadataEPN23sceMetadataReaderWriter8MetadataE` | `835p-ywJWEY` |
| `_ZN15AbstractStorage14YoutubeContent13ParseMetadataEPN23sceMetadataReaderWriter8MetadataES3_` | `iYDibzjdBf8` |
| `_ZN15AbstractStorage14YoutubeContent4OpenEit` | `gID4V0+Uk+k` |
| `_ZN15AbstractStorage14YoutubeContent5CloseEv` | `BbkdawQaHyg` |
| `_ZN15AbstractStorage14YoutubeContent5WriteEPKvlPl` | `95+sJrPf30A` |
| `_ZN15AbstractStorage14YoutubeContent8SeekByteEliPl` | `d5LwAYdjVqk` |
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
| `_ZN15AbstractStorage14YoutubeStorageD0Ev` | `VkzubmxnY7s` |
| `_ZN15AbstractStorage14YoutubeStorageD1Ev` | `D5iaQceaLcI` |
| `_ZN15AbstractStorage14YoutubeStorageD2Ev` | `hfifwAJDCvU` |

- [Older callable NIDs](../../../core/libs/prx/libSceAbstractYoutube/Export.cpp): 20 older exports are named after the mangled symbols the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NIDs, and the NID of each name matches. Their signatures and behavior are unverified. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.

### Exports of the 13.40 module

- [Unnamed exports](../../../core/libs/prx/libSceAbstractYoutube/Export.cpp) (libSceAbstractYoutube) - the 13.40 module exports 27 NIDs that this library did not declare, listed below. They are exported as `libSceAbstractYoutubeUnknown_<nid>_nid_no_patch` under the no-parameter import-resolution convention. Nine of them return at once without reading an argument, with the values in the table; the other eighteen throw through `NotImplemented_nid_no_patch` and their parameters, return types and behavior are unknown. Read from disassembly, not measured on a console.

| Export NID | 13.40 body |
| --- | --- |
| `26+qh3cw-Rk` | `0x809E0005` |
| `2l4I9SCoQGo` | not immediate |
| `2spIcbz9a4c` | not immediate |
| `6A3QZpbCzqI` | not immediate |
| `Bo7s49MExf4` | not immediate |
| `C8lFlQympVw` | `0` |
| `Cz-k0+3Ko-Y` | not immediate |
| `Ds9ID4F7dNc` | not immediate |
| `FkpEJ7DfAtE` | not immediate |
| `GGzJcpDXRz4` | not immediate |
| `HxZv0UgY+7E` | not immediate |
| `Ix3dt-HaP9s` | not immediate |
| `NBDWs6iIqNE` | `0x809E0005` |
| `Pk1mFgNtLEM` | `0x809E0005` |
| `RaO8sKI7L44` | `0x809E0005` |
| `UJyqV64-qNQ` | `0x809E0005` |
| `UqZVYj1m2m4` | not immediate |
| `aVE9tVODTHw` | `0x809E0005` |
| `cobO1RmAxN4` | not immediate |
| `gsLtb-uKeH8` | not immediate |
| `hdVPPVnBbZU` | not immediate |
| `oWpmMJAVnI0` | not immediate |
| `qrZ8UFLI1Us` | not immediate |
| `uk6wVMRO9c0` | not immediate |
| `uyA49-ae2+k` | not immediate |
| `zNGleAOPtNs` | `0x809E0005` |
| `zp94FyokDLI` | `0x809E0005` |
