# libSceGifParser

## change [Firmware 13.42 exports]

- [Firmware 13.42 exports](../../../core/libs/prx/libSceGifParser/Export.cpp) (libSceGifParser) - the 13.42 thumbnail NID `DTWP8QPsnTM` (`gifParserCreateThumbnail2`, read on #21873) is declared but throws: it hands its paths and thumbnail record to libSceImageUtil's `SaveJpegFromFile` (`oOjUpbj5GyI`), which is not implemented. The [13.42 GIF metadata reader](../../../core/libs/prx/libSceGifParser/ReadMetadata.cpp) throws for a caller-supplied key list. The [firmware runs](https://github.com/boykopovar/AnyPS5/issues/21873#issuecomment-6961198994) used the default key list, simulated file timestamps and a host allocator. The setter's return type and failure behavior, allocation sizes, mixed GIF87a block sequences and malformed block errors beyond those inputs have not been established. Keys and values are borrowed for the setter call. Unreadable arguments or a missing allocator throw.
