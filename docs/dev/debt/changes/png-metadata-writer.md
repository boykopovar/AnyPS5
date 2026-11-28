# libScePngParser

## change [Firmware 13.42 exports]

- [Firmware 13.42 exports](../../../core/libs/prx/libScePngParser/Export.cpp) (libScePngParser) - the 13.42 `libScePngParser.sprx` exports seven function NIDs; `suBT1aYALqA` (`pngParserFinalize`) is the only one also in the 3.20 set above, and the thumbnail export `1rmPs1bYWKA` (`CreateThumbnail2`) is not declared: it saves through libSceImageUtil. Read from disassembly, not measured on a console: `0Imz9zU-Wgk` returns `0x809D4019` and `5XfcHLzGE6E` returns `0x809D4054`; `vJpgNFIBlCk` copies the caller's 0x58-byte configuration into both of the module's copies and returns 0; `suBT1aYALqA` clears the first copy and returns 0. The configuration is eleven function pointers that the module's helpers jump through without a null check. A null or unreadable configuration, which the module reads through, throws.

## add Callable exports

- [pLHjV7MzAKI](../../../core/libs/prx/libScePngParser/WriteMetadata.cpp) (libScePngParser) - Cronos is unmodelled. The writer uses the ordinary console's temporary directory. The precise allocator calls inside the 4 MiB workspace, guest ownership changes and error codes for temporary-file, storage and replacement failures are unverified. A failed replacement preserves the source. The firmware's positive getter result other than 1 and malformed callback values are unverified and throw. The observations are from [13.42 running with stand-ins](https://github.com/boykopovar/AnyPS5/issues/21873#issuecomment-6961781466); the console itself was not run.
