# libScePngDec technical debt

### Functional

- [libScePngDec](../../../core/libs/prx/libScePngDec/Export.cpp): the supplied module confirms the work-memory size formula, 16-byte handle alignment, the first 24 bytes of its context, and Create/QueryMemorySize validation order. The host now exposes these values but still allocates decoder working storage on the host instead of using the guest work region.
- [scePngDecDecode](../../../core/libs/prx/libScePngDec/Export.cpp): 16-bit output and the original's accepted pixel format 132 remain unsupported and throw. Malformed-image diagnostics and decoder scratch-memory behavior are not fully matched to the supplied module.

### Unknown function info

- [scePngDecDecodeWithInputControl](../../../core/libs/prx/libScePngDec/Export.cpp) (libScePngDec) - its name and NID are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePngDec.c); the no-parameter prototype follows [shadPS4's pngdec.h](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/libpng/pngdec.h#L76) and is not measured on a console. It is declared without parameters solely to resolve known imports; this is not an ABI claim.
