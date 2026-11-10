# libSceOpusCeltDec technical debt

### Silent stubs

- [sceOpusCeltDecCreate](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp) (libSceOpusCeltDec) - creates a standalone CELT decoder without an AJM context while preserving errno; sample rates and channel counts are validated against libopus requirements. Context ownership and AJM hardware error codes remain unmodeled.

### Unknown function info

- [sceOpusCeltDecTerminate](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp): the non-null path forwards the AJM finalization result, but the null path returns without setting the return register. Its source return type cannot be established from this callee alone. The existing integer return is retained.

### Functional

- [libSceOpusCeltDec](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp): the supplied module confirms the six exported argument lists, read-only context inputs, 640-byte state allocation, mono/stereo channels, five sample rates, byte-sized decode capacity and byte-count return. The implementation still keeps decoder state outside guest memory and uses host contexts instead of AJM contexts. AJM error propagation, resource ownership and termination with live decoders are not implemented. Wrapper validation errors and their precedence are recovered.
- [sceOpusCeltDecDecode](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp): the original forwards packets to AJM codec 16 without a local CELT-only check. The backend response to SILK/hybrid packets is unresolved; packets whose TOC configuration is below 16 still throw. Decoding uses the bundled libopus decoder of [OpusDecoderLibrary](../../../core/libs/prx/libSceOpusDec/include/OpusDecoder.hpp) and shares its limits. Packet loss concealment and asynchronous AJM execution are not implemented.
- [sceOpusCeltDecCtl](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp): handled with sceOpusDecCtl and has the same unmeasured gain, final range and request -1 gaps, see [libSceOpusDec](libSceOpusDec.md). It accepts 4028, 4029, 4031, 4034/4045 and 4046/4047 only; the phase inversion range error is +0x11 instead of +0x1B.
