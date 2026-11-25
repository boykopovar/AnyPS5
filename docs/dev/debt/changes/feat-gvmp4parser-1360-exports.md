# libSceGvMp4Parser

## add Functional

- [libSceGvMp4ParserUnknown_PJigiYrhUwQ_nid_no_patch, libSceGvMp4ParserUnknown_tcsvdFwGn9U_nid_no_patch, libSceGvMp4ParserUnknown_iPAZM6la8Gk_nid_no_patch](../../../core/libs/prx/libSceGvMp4Parser/Export.cpp) - Verified against firmware 13.60 `libSceGvMp4Parser.sprx`: `iPAZM6la8Gk` returns `0x809D4054` after path copy when initialized; `tcsvdFwGn9U` shares the `0x51E0` path-extension, `sceKernelStat`, and `.MP4`/`.M4V`/`.MGV`/`.MNV` validation gate with `PJigiYrhUwQ`, returning `0x809D4203` on `ENOENT`/`EINVAL` and `0x809D4201` on other stat failures or unsupported extensions while throwing before `sceKernelStat` when the filename lacks an extension.
