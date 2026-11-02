# libSceVideoDecoderArbitration technical debt

### Functional

- [sceVideoDecoderArbitrationInitialize](../../../core/libs/prx/libSceVideoDecoderArbitration/Export.cpp) (libSceVideoDecoderArbitration) - tracks initialization state atomically; duplicate initialization throws `std::logic_error`. Hardware error reporting is unmeasured.

### Unknown function info

- [sceVideoDecoderArbitrationAcceptEvent, sceVideoDecoderArbitrationEnable and sceVideoDecoderArbitrationEnableSuspendMode](../../../core/libs/prx/libSceVideoDecoderArbitration/Export.cpp) (libSceVideoDecoderArbitration) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVideoDecoderArbitration.c), but no credible prototypes were found. Parameterless declarations resolve imports; uninitialized calls throw `std::logic_error` and initialized paths throw `NotImplemented_nid_no_patch`.
