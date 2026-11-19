# libScePsm

## change [ScePsmMonoArrayAddrWithSize, ScePsmMonoArrayLength, ScePsmMonoDomainGet

- [ScePsmMonoDomainGet](../../../core/libs/prx/libScePsm/Export.cpp) - name and NID match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signature and behavior are unverified. The no-parameter declaration resolves imports and throws through `NotImplemented_nid_no_patch` when called; it needs a Mono domain, which nothing here models.

## change [ScePsmMonoStringChars, ScePsmMonoStringFromUtf16

- [ScePsmMonoStringFromUtf16, ScePsmMonoStringNew, ScePsmMonoStringNewWrapper](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called; creating a string needs the domain's `System.String` vtable, which nothing here models.

## change [ScePsmMonoStringToUtf8, _ZN3sce3pss4core14IntHandleTable13NextIntHandleEv

- [_ZN3sce3pss4core14IntHandleTable13NextIntHandleEv, _ZN3sce3pss4core8graphics14NativeGraphics18ReleaseVideoMemoryEPv, _ZN3sce3pss4core8graphics14NativeGraphics19AllocateVideoMemoryEjj, _ZN3sce3pss4core8graphics14NativeGraphics19AllocateVideoMemoryEjjPv](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

## add Unknown function info

- [ScePsmMonoArrayAddrWithSize, ScePsmMonoArrayLength, ScePsmMonoFree, ScePsmMonoGcWbarrierGenericStore, ScePsmMonoStringChars, ScePsmMonoStringLength, ScePsmMonoStringToUtf8](../../../core/libs/prx/libScePsm/Export.cpp) - not read from `libScePsm.sprx` or measured on a console: prototypes follow the `mono_*` embedding functions the names spell out, on the 64-bit `MonoString` and `MonoArray` layouts at [mono/mono 0f53e9e](https://github.com/mono/mono/blob/0f53e9e151d92944cacab3e24ac359410c606df6/mono/metadata/object-internals.h) (array length read as 32 bits, the default without `MONO_BIG_ARRAYS`). `StringToUtf8` follows that commit's eglib: null for a null string or an unpaired surrogate before the first NUL, where conversion stops. The write barrier is a plain store, as no Mono collector runs here, and `Free` uses the C allocator `StringToUtf8` uses; the console's Mono version and allocator are unknown.
