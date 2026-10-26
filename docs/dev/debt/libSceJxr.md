# libSceJxr technical debt

### Unknown function info

- [PixelFormatLookup](../../../core/libs/prx/libSceJxr/Export.cpp) is listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceJxr.c) and was cross-checked against firmware 13.60. Its signature and behaviour remain unverified; the declaration uses the no-parameter unknown-signature convention and throws through `NotImplemented_nid_no_patch`.

### Functional

- [PKFree](../../../core/libs/prx/libSceJxr/Export.cpp) (libSceJxr) follows Microsoft's jxrlib [JXRGlue.c](https://github.com/4creators/jxrlib/blob/f7521879862b9085318e814c6157490dd9dbbdb4/jxrgluelib/JXRGlue.c#L53-L62), whose four exported names the module shares: a non-null `ppv` has `*ppv` released with the guest `free` and set to null, and the result is always `WMP_errSuccess` (0) as a 64-bit `ERR` (`long`). That the console's module is built from jxrlib, and that its `PKFree` releases through libc's `free`, is inferred from the export names and not checked against the console's code. Closes when: the console's `PKFree` is disassembled or measured on a PS5

- [readIS and writeIS](../../../core/libs/prx/libSceJxr/Export.cpp) use the non-ARM, LP64 `BitIOInfo` and `WMPStream` ABI and packet algorithm from [Microsoft's JPEG XR reference implementation](https://github.com/4creators/jxrlib/blob/f7521879862b9085318e814c6157490dd9dbbdb4/image/sys/strcodec.c#L1090-L1177). Console structure layouts and execution remain unverified. Profiling callbacks are omitted as in the reference Unix build with `DISABLE_PERF_MEASUREMENT`; the codec argument is unused. Invalid null state or required callbacks throw instead of dereferencing them. No codec is instantiated by these helpers; callers must supply the reference ring buffer and guest-ABI stream callbacks.
