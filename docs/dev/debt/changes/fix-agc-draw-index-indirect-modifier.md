# libSceAgc

## add Functional

- [sceAgcDcbDrawIndexIndirect](../../../core/libs/prx/libSceAgc/DcbDraw/src/DrawIndexed.cpp) (libSceAgc) - modifier bit 1 names where the CP writes the record's firstIndex: [prosper's console measurements](https://github.com/mattias800/prosper/blob/d44c89db977a162af271d986a8d1febbc31e702f/prosper/tests/data/console_oracle/agcbuild.golden.tsv) (captured 2026-10-08, cases agcbuild_DcbDrawIndexIndirect_a and _b) put the start-index register `sgprBase + ((modifier >> 14) & 0x1f)` in the second packet word's bits 31:16 and set bit 28 of the third word, and clear both fields when the bit is clear; sceAgcDcbDrawIndirect clears them even with the bit set (cases agcbuild_DcbDrawIndirect_a and _b). What bit 1 adds to the multi and auto forms is not measured: every measured modifier there has the bit clear (0x55555555, 5 and 0), so those packets keep the bit-1-clear encoding and the driver still refuses start-index bits in their packets.
