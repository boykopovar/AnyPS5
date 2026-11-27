# libScePngDec

## add Functional

- [Incomplete PNG output](../../../core/libs/prx/libScePngDec/Export.cpp): the early-IDAT row-loss gap remains for Adam7 and indexed output. Non-interlaced RGBA8 and BGRA8 keep completed rows and return the decode error, following [the firmware 13.42 module comparison](https://github.com/boykopovar/AnyPS5/pull/12903). Partial output on other decompression errors was measured in one run of a PS5 (firmware 11.60, payload process; a title process may differ), all with `0x80690012` on 4x4 images that are not interlaced, rows written out of 4: a stream ending after 2 rows: 2 (RGBA8) and 2 (BGRA8); the same without IEND: 2 (RGBA8); all 4 rows with a wrong zlib Adler-32: 3 (RGBA8); a file ending inside the IDAT chunk: 0 (RGBA8); a stored block without data: 0 (RGBA8). A negative input-callback result still propagates before output is written. Filter type 7 remains unsupported.
