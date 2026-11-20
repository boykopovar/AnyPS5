# libSceIcu

## add Functional

- [Data swapper](../../../core/libs/prx/libSceIcu/DataSwap.cpp) (libSceIcu) - eight exports ported from ICU 68.2's `udataswp.cpp`: the swapper open, close, input-data open, header swap, invariant string block swap, endianness readers and the error printer, with the array copy and swap primitives the struct carries. Only the ASCII charset family is accepted: the EBCDIC branches need the `uinvchar.cpp` tables, which this build does not carry, so an EBCDIC request answers `U_UNSUPPORTED_ERROR` instead of converting silently. The type-specific payload swappers (`ucnv_swap`, `ures_swap`, `ucol_swap`) are separate ports and still answer `NotImplemented_nid_no_patch`.
