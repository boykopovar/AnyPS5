# libSceJpegDec technical debt

### Unknown function info

- [sceJpegDecDecodeWithInputControl](../../../core/libs/prx/libSceJpegDec/Export.cpp) (libSceJpegDec) - its name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceJpegDec.c), but no credible prototype was found. It is declared without parameters solely to resolve known imports; this is not an ABI claim

### Functional

- [libSceJpegDec](../../../core/libs/prx/libSceJpegDec/Export.cpp): work-memory size, alignment, creation checks, image-info fields, progressive coefficient-buffer capacity, row-buffer capacity and scaled dimensions are recovered from the native module. The host still allocates its decoder storage separately rather than using the guest work and coefficient regions. Malformed-header diagnostics and output mutation on failure are not fully matched.
- [sceJpegDecDecode](../../../core/libs/prx/libSceJpegDec/Export.cpp): decode mode 1 and pixel formats 9, 10 and 11 remain unimplemented and throw. Exact decoded pixel values and color conversion are not verified against the native decoder. One title (PPSA29866) disassembled for this entry has one call site (0x2c111d); it passes a null `imageInfo` and a parameter structure with decode mode 0, down-scale 1 and pixel format 0, and hands the result to another function whose handling of a negative result was not read. In the parameter structure built around this call site, mode 1 and formats 9, 10 and 11 do not show up; whether a down-scale of 1 means no scaling was not compared.
