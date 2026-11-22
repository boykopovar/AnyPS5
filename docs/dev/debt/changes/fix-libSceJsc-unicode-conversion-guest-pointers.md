# libSceJsc

## add Functional

- [WTF Unicode conversion guest pointer validation](../../../core/libs/prx/libSceJsc/src/WtfUnicodeConversion.cpp) - console behaviour on inaccessible or read-only cursor pointers, `sourceAllAscii` flags, or source/target buffers in `WTF::Unicode::convertLatin1ToUTF8`, `convertUTF16ToUTF8`, `convertUTF8ToUTF16`, and `convertUTF8ToUTF16ReplacingInvalidSequences` is unmeasured. Closes when: console behaviour on invalid guest pointers in these Unicode conversion exports is measured.
