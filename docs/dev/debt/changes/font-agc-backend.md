# libSceFontGs.native

## change [sceFontSelectGraphicsAgc]

- [sceFontSelectGraphicsAgc](../../../core/libs/prx/libSceFontGs.native/AgcBackend.cpp) (libSceFontGs.native) - exposes the revision 5 backend observed in the FontGsm module shipped with Stupid Never Dies (PPSA28050). Index zero selects its service, with a writable instance slot, 21 ordered entries, two scalar limits, and a no-op termination callback. Older 4.03 firmware returned null for all indices. The texture callback builds a sampled image descriptor from an uncompressed, single-layer font color target. Other target layouts, formats and tile modes throw. Seventeen initialization and drawing operations still throw with their individual opcodes. Guest memory copy failures and a missing sink for a nonnull dispatch raise host argument errors. Those invalid memory cases have no measured console error result.
