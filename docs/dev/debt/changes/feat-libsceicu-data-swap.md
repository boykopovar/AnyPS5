# libSceIcu

## add Functional

- [ICU data swappers](../../../core/libs/prx/libSceIcu/DataSwap.cpp) (libSceIcu) - not compared on a console. `ucol_swap`, `ucol_swapInverseUCA`, `ucol_looksLikeCollationBinary`, `unorm2_swap`, `ucnv_swapAliases`, `uchar_swapNames`, `ubrk_swap` and `udict_swap` call ICU 78.1 with a host copy of the guest's `UDataSwapper` whose functions call the guest's; 78.1's swap code matches ICU 68.2 except that `unorm2_swap` accepts format version 5, which is refused here as in 68.2. Temporary tables of `uchar_swapNames` and `ucnv_swapAliases` come from the host allocator, not from functions set with `u_setMemoryFunctions`. The swapper's flags are read once per call. On Windows hosts the guest's `printError` gets a System V `va_list` over the host's variadic arguments, which holds for the integer and pointer arguments ICU passes; tests run only on Linux. `udata_openSwapper` is not implemented, so a guest has to fill its own swapper.
