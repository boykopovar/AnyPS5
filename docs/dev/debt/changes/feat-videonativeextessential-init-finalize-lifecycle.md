# libSceVideoNativeExtEssential

## add Functional

- [sceVideoNativeExtEssential lifecycle and instance validation](../../../core/libs/prx/libSceVideoNativeExtEssential/Export.cpp) - Modeled from firmware 13.60 `libSceVideoNativeExtEssential.sprx` disassembly, not run on a console: `Initialize` probes the allocator callback table (`+0x40..+0x58`), marks init (`IsInit() == 1`), and returns `0xF0000002` on re-init; `Finalize` resets init and returns `0`; `CreateDecInstance` validates registered decoder types (`2`, `4`, `0x2715`, `0x2718`, `0x271A`, `0x271C`, returning `0xF0000008` otherwise) before `NotImplemented_nid_no_patch`; `DestroyDecInstance` rejects null or untracked handles with `0xF0000003` after init.
