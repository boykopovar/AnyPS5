# libc

## add Functional

- [scalbn](../../../core/libs/prx/libc/src/Math.cpp) (libc) - console execution, other SDK revisions and unmasked exception delivery are unverified. Double scaling uses x87 arithmetic and double conversion, preserving `errno` and SSE state while accumulating x87 exception flags. This follows the owned PPSA21564 bundled `libc.prx` export at 0x48bd0, read from that module and compared natively on Linux. SSE DAZ/FTZ do not flush its inputs or outputs. The caller's x87 rounding mode controls subnormal and overflow results; double `ldexp` has a separate contract and is unchanged.
