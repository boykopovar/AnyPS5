# libSceSrcUtl technical debt

### Unknown function info

- [sceSrcUtilityFinalize, sceSrcUtilityFlush, sceSrcUtilityGetDelaySamples, sceSrcUtilityGetHandleSize, sceSrcUtilityGetPriority, sceSrcUtilityInitialize, sceSrcUtilityInitializeEx, sceSrcUtilityResample and sceSrcUtilitySetPriority](../../../core/libs/prx/libSceSrcUtl/Export.cpp) (libSceSrcUtl) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSrcUtl.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

- [sceSrcUtilityCreateEx, sceSrcUtilityDeleteEx, sceSrcUtilityGetAjmContext, sceSrcUtilityTerminateEx](../../../core/libs/prx/libSceSrcUtl/Export.cpp) throw through `NotImplemented_nid_no_patch`. Names and NIDs are bound by the [13.60 firmware inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscesrcutl-970010b531.jsonl); signatures and behaviour remain unknown. Parameterless `int` declarations are import-resolution placeholders, not confirmed ABI signatures. Their service behaviour is unimplemented.
