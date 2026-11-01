# libSceVideoNativeExtEssential technical debt

### Additional callable exports

- [Native video export `iwAQhvb9BvY`](../../../core/libs/prx/libSceVideoNativeExtEssential/Export.cpp) is a function record with qualifier `K#A` in older and later version-labelled modules. Both map library ID 10 and module ID 0 to `libSceVideoNativeExtEssential`. Older module words: `0x08540001`, `0x03200004`; later: `0x13590001`, `0x13600007`. Both contain six function NIDs; sorted-LF SHA-256: `2c1bdced348433c9cb792b70a0f043d932e6f822157250d5256bb46c8867bd86`, using the [static ELF membership procedure](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298). Labels do not authenticate retail provenance; the digest identifies the NID set only. Independent verification requires a matching image. The export name, signature and behavior are unverified; this declaration throws.


### Unknown function info

- [sceVideoNativeExtEssential_CreateDecInstance, sceVideoNativeExtEssential_DestroyDecInstance, sceVideoNativeExtEssential_Finalize, sceVideoNativeExtEssential_Initialize and sceVideoNativeExtEssential_IsInit](../../../core/libs/prx/libSceVideoNativeExtEssential/Export.cpp) (libSceVideoNativeExtEssential) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVideoNativeExtEssential.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
