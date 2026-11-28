# libSceLibcInternal

## add Functional

- [std::_Throw](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (`pDFe-IgbTPg`) calls virtual `what()` at guest vtable offset 16, flushes guest streams, prints `%s(%u) : error : %s\n` with the returned message, zero and `exception` to the fixed standard-error object, then aborts. This fatal diagnostic sequence matches three distinct title-shipped SDK modules; it does not unwind a C++ exception. Unreadable objects, unreadable vtable entries and null callbacks are refused rather than reproducing a guest fault. Other SDK variants and console hardware are unverified. Focused tests cover virtual dispatch, flush order, empty/percent/multiline/long messages, a one-word object beside a guard page, and abort termination on Linux; Windows fatal subprocess behavior is untested. No gameplay unblock is established.
