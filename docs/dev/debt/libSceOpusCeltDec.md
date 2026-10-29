# libSceOpusCeltDec technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp) - One entry observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `sceOpusCeltDecGetAjmContext` | `Z4-1fNubfUo` |


### Unknown function info

- [sceOpusCeltDecCreate, sceOpusCeltDecCtl](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp) (libSceOpusCeltDec) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceOpusCeltDec.c), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim.
- [sceOpusCeltDecTerminate](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp): the non-null path forwards the AJM finalization result, but the null path returns without setting the return register. Its source return type cannot be established from this callee alone. The existing integer return is retained.

### Functional

- [libSceOpusCeltDec](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp): the supplied module confirms the six exported argument lists, read-only context inputs, 640-byte state allocation, mono/stereo channels, five sample rates, byte-sized decode capacity and byte-count return. The implementation still keeps decoder state outside guest memory and uses host contexts instead of AJM contexts. AJM error propagation, resource ownership and termination with live decoders are not implemented. Wrapper validation errors and their precedence are recovered.
- [sceOpusCeltDecDecode](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp): the original forwards packets to AJM codec 16 without a local CELT-only check. The backend response to SILK/hybrid packets is unresolved; packets whose TOC configuration is below 16 still throw. Decoding uses the bundled libopus decoder of [OpusDecoderLibrary](../../../core/libs/prx/libSceOpusDec/include/OpusDecoder.hpp) and shares its limits. Packet loss concealment and asynchronous AJM execution are not implemented.
