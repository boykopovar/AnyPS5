# libScePSNowGkp technical debt

### Unknown function info

- [sceCloudClientCancelRequestGame, sceCloudClientReset, sceCloudClientSetAuthCodes, sceCloudClientTestConnection](../../../core/libs/prx/libScePSNowGkp/Export.cpp) (libScePSNowGkp) - names and NIDs verified from the pinned [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePSNowGkp.c) (`libScePSNowGkp.c`), with NID match confirmed via `tools/nid_names.py` `compute_nid`. Signatures, parameter layouts, and runtime behavior remain unverified; all paths throw through `NotImplemented_nid_no_patch`.
