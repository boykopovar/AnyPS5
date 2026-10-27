# libSceAvcap2 technical debt

### Unknown function info

The 7 exports in [Export.cpp](../../../core/libs/prx/libSceAvcap2/Export.cpp) are declared from the NIDs that the PS5 homebrew project [HearBridge-PS5](https://github.com/X-F1REBALL-X/HearBridge-PS5/blob/main/src/avcap2.c) resolves from `libSceAvcap2` (`avcap2.c`, lines 23-29); each NID was checked to equal the hash of the `sceAvcap2*` name given here. Whether these are all the exports of the PS5 module is unknown, and prototypes and behaviour are unverified: every function takes no parameters in the import-resolution convention and throws through `NotImplemented_nid_no_patch`.

- [sceAvcap2Initialize](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID svzPXluOz8U)
- [sceAvcap2Terminate](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID gojqghDU+1Y)
- [sceAvcap2OpenAudio](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID 3nHpE7Dp5SE)
- [sceAvcap2Close](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID tR7gPe1i8hw)
- [sceAvcap2Start](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID 8CGNCwBsItI)
- [sceAvcap2Stop](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID Z5dKK0xnQ8g)
- [sceAvcap2ReadAudio](../../../core/libs/prx/libSceAvcap2/Export.cpp) (libSceAvcap2) - unknown signature (NID WhsHggqdwcg)
