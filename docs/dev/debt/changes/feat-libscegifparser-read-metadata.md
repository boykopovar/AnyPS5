# libSceGifParser

## change [Firmware 13.42 exports]

- [Firmware 13.42 exports](../../../core/libs/prx/libSceGifParser/Export.cpp) (libSceGifParser) - the 13.42 write and thumbnail NIDs `SZeQGmAx+N0` and `DTWP8QPsnTM` are undeclared. The [13.42 GIF metadata reader](../../../core/libs/prx/libSceGifParser/ReadMetadata.cpp) throws for a caller-supplied key list. The [firmware runs](https://github.com/boykopovar/AnyPS5/issues/21873#issuecomment-6961198994) used the default key list, simulated file timestamps and a host allocator. The setter's return type and failure behavior, allocation sizes, mixed GIF87a block sequences and malformed block errors beyond those inputs have not been established. Keys and values are borrowed for the setter call. Unreadable arguments or a missing allocator throw.
