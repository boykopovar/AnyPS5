# libSceVideoNativeExtEssential technical debt

### Additional callable exports

- [Native video export `iwAQhvb9BvY`](../../../core/libs/prx/libSceVideoNativeExtEssential/Export.cpp) is a function record with qualifier `K#A` in older and later version-labelled modules. Both map library ID 10 and module ID 0 to `libSceVideoNativeExtEssential`. Older module words: `0x08540001`, `0x03200004`; later: `0x13590001`, `0x13600007`. Both contain six function NIDs; sorted-LF SHA-256: `2c1bdced348433c9cb792b70a0f043d932e6f822157250d5256bb46c8867bd86`, using the [static ELF membership procedure](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298). Labels do not authenticate retail provenance; the digest identifies the NID set only. Independent verification requires a matching image. The export name and the meaning of its arguments are unverified; past the checks under Functional it throws.


### Unknown function info

- [sceVideoNativeExtEssential_Finalize](../../../core/libs/prx/libSceVideoNativeExtEssential/Export.cpp) (libSceVideoNativeExtEssential) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVideoNativeExtEssential.c), but no credible prototype was found. The declaration has no parameters solely to resolve the import; this is not an ABI claim

### Functional

- [sceVideoNativeExtEssential_IsInit, sceVideoNativeExtEssential_CreateDecInstance, sceVideoNativeExtEssential_DestroyDecInstance and sceVideoNativeExtEssential_Initialize](../../../core/libs/prx/libSceVideoNativeExtEssential/Export.cpp) (libSceVideoNativeExtEssential) - only the answers before initialization are implemented. In firmware 13.42 (read from disassembly, not measured on a console) the library is initialized while a flag byte is set, which only `sceVideoNativeExtEssential_Initialize` sets, and it throws here once its checks pass. `IsInit` returns that flag (0). `CreateDecInstance` stores a null pointer through its second argument and returns `0xF0000000`; `DestroyDecInstance` returns `0xF0000000` without reading its argument; `Initialize` returns `0xF0000003` for a null parameter, a zero first quadword or a 32-bit value at offset 8 other than 1. An unreadable or unwritable argument throws, as the module reads through it. Only the arguments read are declared, named here.
- [Native video export `iwAQhvb9BvY`](../../../core/libs/prx/libSceVideoNativeExtEssential/Export.cpp) (libSceVideoNativeExtEssential) - returns `0xF0000003` for a null 7th argument and then `0xF0000008` for a first argument other than 2, 3 or 4, as firmware 13.42 does (read from disassembly); otherwise it forwards to the decoder and throws here. The widths of the 2nd to 6th arguments are not settled; they are declared as 32-bit values only so the 7th lands on the stack.
