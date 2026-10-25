# libSceJpegDec technical debt

### Unknown function info

- [sceJpegDecDecodeWithInputControl](../../../core/libs/prx/libSceJpegDec/Export.cpp) (libSceJpegDec) - its name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceJpegDec.c), but no credible prototype was found. It is declared without parameters solely to resolve known imports; this is not an ABI claim

### Functional

- [libSceJpegDec](../../../core/libs/prx/libSceJpegDec/Export.cpp): work-memory size, alignment, creation checks, image-info fields, progressive coefficient-buffer capacity, row-buffer capacity and scaled dimensions are recovered from the native module. The host still allocates its decoder storage separately rather than using the guest work and coefficient regions. Malformed-header diagnostics and output mutation on failure are not fully matched.
- [sceJpegDecDecode](../../../core/libs/prx/libSceJpegDec/Export.cpp): decode mode 1 and pixel formats 9, 10 and 11 remain unimplemented and throw. Exact decoded pixel values and color conversion are not verified against the native decoder.
