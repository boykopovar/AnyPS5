# libScePngDec technical debt

### Functional

- [libScePngDec](../../../core/libs/prx/libScePngDec/Export.cpp): the supplied module confirms the work-memory size formula, 16-byte handle alignment, the first 24 bytes of its context, and Create/QueryMemorySize validation order. The host now exposes these values but still allocates decoder working storage on the host instead of using the guest work region.
- [scePngDecDecode](../../../core/libs/prx/libScePngDec/Export.cpp): the original's accepted pixel format 132 remains unsupported and throws. The input bit-depth limit and 16-to-8-bit conversion follow [Prosper's PS5 3.20 module analysis](https://github.com/mattias800/prosper/blob/550886e3677e4a2ef81ad1603bf313f6a9696de2/prosper/src/hle/util/hle_pngdec.cpp#L412-L414), and the large-dimension image-info requirement and its validation order follow [Prosper's PS5 3.20 module analysis](https://github.com/mattias800/prosper/blob/550886e3677e4a2ef81ad1603bf313f6a9696de2/prosper/src/hle/util/hle_pngdec.cpp#L385-L396), not console measurements here. The supplied 13.42 module accepts row filter type 7 and leaves the rows decoded before an error when the IDAT data ends early; here both decode nothing and return `0x80690012`. Decoder scratch-memory behavior is not matched to the supplied module.

### Unknown function info

- [scePngDecDecodeWithInputControl](../../../core/libs/prx/libScePngDec/Export.cpp) (libScePngDec) - its name and NID are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePngDec.c); the no-parameter prototype follows [shadPS4's pngdec.h](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/libpng/pngdec.h#L76) and is not measured on a console. It is declared without parameters solely to resolve known imports; this is not an ABI claim.
