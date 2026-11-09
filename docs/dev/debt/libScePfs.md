# libScePfs technical debt

### Unknown function info

- [scePfsGetBlockTableReadCommand, scePfsGetHeaderReadCommand, scePfsGetPreadBlockRange, scePfsGetSingleBlockReadCommand, scePfsGetUncompressedSize, scePfsGetWorkBufferSize, scePfsPread, scePfsPreadDecompressBlock, scePfsValidate](../../../core/libs/prx/libScePfs/Export.cpp) (libScePfs) - names and NIDs verified from pinned [ps5rs stubs](https://github.com/claimore22/ps5rs/tree/5f252a658ff470bcb2b6cd15205fd8ca64ae4477/data/stubs/by_library) (`libScePfs.txt`), with NID match confirmed via `tools/nid_names.py` `compute_nid`. Signatures, parameter layouts, and runtime behavior remain unverified; all paths throw through `NotImplemented_nid_no_patch`.
