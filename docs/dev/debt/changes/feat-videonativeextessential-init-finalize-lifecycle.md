# libSceVideoNativeExtEssential

## add Functional

- [sceVideoNativeExtEssential lifecycle and instance validation](../../../core/libs/prx/libSceVideoNativeExtEssential/Export.cpp) - Modeled from firmware 13.60 `libSceVideoNativeExtEssential.sprx` disassembly, not run on a console: `Initialize` validates non-null allocator callbacks at `+0x40..+0x58`, marks init (`IsInit() == 1`), and returns `0xF0000002` on re-init; `Finalize` resets init and returns `0`; `CreateDecInstance` zeroes `*instance` and calls `NotImplemented_nid_no_patch` after init; `DestroyDecInstance` rejects null or untracked handles with `0xF0000003` after init.
