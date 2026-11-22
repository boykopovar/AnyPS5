# libSceJsc

## add Functional

- [WTF Unicode conversion guest pointer validation](../../../core/libs/prx/libSceJsc/src/WtfUnicodeConversion.cpp) - console behaviour on inaccessible or read-only cursor pointers, `sourceAllAscii` flags, or source/target buffers in `WTF::Unicode::convertLatin1ToUTF8`, `convertUTF16ToUTF8`, `convertUTF8ToUTF16`, and `convertUTF8ToUTF16ReplacingInvalidSequences` is unmeasured; streaming reads up to one bounded stack chunk (bounded by remaining target capacity and capped at 4096 bytes / 2048 UTF-16 units), so an inaccessible byte inside that lookahead chunk can throw even if conversion would otherwise stop earlier within the chunk. Closes when: console behaviour on invalid guest pointers in these Unicode conversion exports is measured.
