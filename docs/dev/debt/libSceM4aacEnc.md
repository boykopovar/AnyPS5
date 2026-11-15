# libSceM4aacEnc technical debt

### Functional

- [CheckEncoderParameters, CheckEncode, CheckFlush, sceM4aacEncCreateEncoderEx, sceM4aacEncDeleteEncoderEx and libSceM4aacEncUnknown_dyjd6gEaAXw_nid_no_patch](../../../core/libs/prx/libSceM4aacEnc/Export.cpp) (libSceM4aacEnc) - returning ErrorInvalidPointer (0x80A90002) for unmapped or unreadable parameter and context pointers is a host-safety fallback preserving host errno; console behavior on unmapped memory is unmeasured.
- [libSceM4aacEnc](../../../core/libs/prx/libSceM4aacEnc/Export.cpp): every export except `dyjd6gEaAXw` throws through `NotImplemented_nid_no_patch` once its argument checks pass. The 13.42 module (`libSceM4aacEnc.sprx`, sha256 b6bef8401753f6b283edbeab5065aaab6628ed6910be90f6b11b822d5341891c) encodes through libSceAjmi: its initializer creates the global context with it, and encoder creation, encoding, flushing, deletion and clearing are libSceAjmi instance and batch calls, none of which is modelled here. `sceM4aacEncClearContext` and `sceM4aacEncTerminate` throw for every input: the first makes its libSceAjmi call without a check, and the second returns from a null context without setting a return value. The checks and their `0x80A900xx` codes were read from disassembly up to the first libSceAjmi call, not measured on a console.

### Unknown function info

- [sceM4aacEnc*](../../../core/libs/prx/libSceM4aacEnc/Export.cpp) (libSceM4aacEnc) - five signatures follow [Orbital](https://github.com/AlexAltea/orbital/blob/cc408367da6a6ebbf5ac1ac559fefeadc73b2831/tools/ida/db_types.json) and agree with the registers the 13.42 exports read; the other thirteen follow those registers only, so their names for the arguments are not from a header. The `Ex` context is two 32-bit values: the first is passed to every libSceAjmi call, the second holds the encoder instance (-1 for none); the rest of its meaning is unknown, as are the meanings of output format bits 0 and 16 (bit 16 is refused with one channel) and of the priority argument.
- [M4aac encoder export `dyjd6gEaAXw`](../../../core/libs/prx/libSceM4aacEnc/Export.cpp) has no known name; it returns the first value of an `Ex` context.
