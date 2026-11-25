# libSceVorbisDec

## add Functional
- [sceVorbisDecOpenCallbacks, sceVorbisDecRead, sceVorbisDecReadFilter and sceVorbisDecReadFloat](../../../core/libs/prx/libSceVorbisDec/Export.cpp) (libSceVorbisDec) - `initial` in `sceVorbisDecOpenCallbacks` is copied via `GuestCopyIn_nid_no_patch`, output ranges in `sceVorbisDecRead`, `sceVorbisDecReadFilter` and `sceVorbisDecReadFloat` are checked with `GuestWritable_nid_no_patch` before decoding and copied out via `GuestCopyOut_nid_no_patch` (with `sceVorbisDecReadFloat` staging channel pointers and samples in `GuestArena` memory when available), returning `OV_EFAULT` on unreadable or unwritable pointers while preserving `errno`. Whether the console faults or returns `OV_EFAULT` on invalid pointers is unmeasured.
