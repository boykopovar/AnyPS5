# libSceCompanionUtil technical debt

### Unknown function info

- [sceCompanionUtilGetEvent](../../../core/libs/prx/libSceCompanionUtil/Export.cpp) - the PS5 argument contract, event size and alignment remain unverified for the output pointer. The supplied `libSceCompanionUtil.sprx` export `cE5Msy11WhU` at 0x100 only returns `0x80AD0008`, without reading argument registers or memory. The implementation preserves that return and does not access the buffer.
- [sceCompanionUtilGetRemoteOskEvent, sceCompanionUtilInitialize, sceCompanionUtilOptParamInitialize and sceCompanionUtilTerminate](../../../core/libs/prx/libSceCompanionUtil/Export.cpp) (libSceCompanionUtil) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCompanionUtil.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
