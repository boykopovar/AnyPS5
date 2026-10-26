# libSceOpusCeltDec technical debt

### Unknown function info

- [sceOpusCeltDecCreate, sceOpusCeltDecCtl, sceOpusCeltDecDecodeFloat, sceOpusCeltDecDecodeFloatWithPriority, sceOpusCeltDecDecodeWithPriority](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp) (libSceOpusCeltDec) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceOpusCeltDec.c), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim.
- [sceOpusCeltDecTerminate](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp): the non-null path forwards the AJM finalization result, but the null path returns without setting the return register. Its source return type cannot be established from this callee alone. The existing integer return is retained.

### Functional

- [libSceOpusCeltDec](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp): the supplied module confirms the six exported argument lists, read-only context inputs, 640-byte state allocation, mono/stereo channels, five sample rates, byte-sized decode capacity and byte-count return. The implementation still keeps decoder state outside guest memory and uses host contexts instead of AJM contexts. AJM error propagation, resource ownership and termination with live decoders are not implemented. Wrapper validation errors and their precedence are recovered.
- [sceOpusCeltDecDecode](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp): the original forwards packets to AJM codec 16 without a local CELT-only check. The backend response to SILK/hybrid packets is unresolved; packets whose TOC configuration is below 16 still throw. Decoding uses the bundled libopus decoder of [OpusDecoderLibrary](../../../core/libs/prx/libSceOpusDec/include/OpusDecoder.hpp) and shares its limits. Packet loss concealment and asynchronous AJM execution are not implemented.
