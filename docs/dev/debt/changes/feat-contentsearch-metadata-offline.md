# libSceContentSearch

## change [Metadata access]

- [Metadata access](../../../core/libs/prx/libSceContentSearch/Export.cpp) (libSceContentSearch) - following firmware 13.60 `libSceContentSearch.sprx` (SHA-256 `3c661bff5fa107e9366fbbbc097187f6e28ce20c5b7ae37466bd22bb4a195494`, disassembly only): `sceContentSearchOpenMetadataByContentId` (`0x1a060`/`0x11630`/`0x1d7a0`) returns `0x809D1003` when no offline record exists; `sceContentSearchGetMetadataFieldInfo` (`0x12af0`) and `sceContentSearchGetMetadataValue` (`0x13180`) return `0x809D1003` for out-of-range ids (`< 0` or `>= 16`) before checking `field` and for in-range unopened ids (`0..15`) on normal fields (slot table `0x40170` empty), while `"timestamp"`, `"timeline_tag"`, and `"constant_tag"` branch before the slot check (permission/IPC-dependent `0x809D1010`/`0x809D0002`) and still throw. `sceContentSearchOpenMetadata` still throws.
