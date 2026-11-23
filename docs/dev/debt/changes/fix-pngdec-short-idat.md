# libScePngDec

## add Functional

- [Incomplete PNG output](../../../core/libs/prx/libScePngDec/Export.cpp): the early-IDAT row-loss gap remains for Adam7 and indexed output. Non-interlaced RGBA8 and BGRA8 keep completed rows and return the decode error, following [the firmware 13.42 module comparison](https://github.com/boykopovar/AnyPS5/pull/12903). Partial output on other decompression errors is unmeasured. A negative input-callback result still propagates before output is written. Filter type 7 remains unsupported.
