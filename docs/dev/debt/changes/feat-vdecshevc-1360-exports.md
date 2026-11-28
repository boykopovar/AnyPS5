# libSceVdecShevc

## add Unknown function info

- [HasNoDecoder inner handle check](../../../core/libs/prx/libSceVdecShevc/Export.cpp) - Updated from firmware 13.60 `libSceVdecShevc.sprx`: each of the nine handle-taking exports checks `handle` and `[handle + 0]` in its vtable thunk (`1` if null), then checks `[handle + 0x10]` in its inner method (`1` if null, e.g. after `sceSdecDestroySwHevc`). Remaining unimplemented fields stay via `NotImplemented_nid_no_patch`.
