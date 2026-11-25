# libc

## add Functional

- [ldexpf and scalbnf](../../../core/libs/prx/libc/src/Math.cpp) (libc) - console execution, other SDK revisions and unmasked exception delivery are unverified. Both use x87 scaling and float conversion, preserving `errno`, SSE state and existing x87 exception flags. The `libc.prx` bundled with PPSA21564 exports both at 0x4ef80 as one x87 implementation; read from that owned module and compared natively on Linux, not measured on a console. SSE DAZ/FTZ do not flush its inputs or outputs; rounding and new exception flags follow x87.

## change [std::__sce_v2 extended-precision helpers]

- [std::__sce_v2 extended-precision helpers](../../../core/libs/prx/libc/src/VersionedExtendedPrecision.cpp) (libc) - console execution, firmware and SDK coverage are unverified. `_Xp_setw`, `_Xp_getw`, `_Xp_setn`, `_Xp_movx`, `_Xp_addh`, `_Xp_addx`, `_Xp_mulh`, `_Xp_mulx`, `_Xp_ldexpx`, their `_FXp` forms and `_Feraise` follow the `libc.prx` bundled with PPSA03671 v01.001.005. They split values into 26-bit (double) or 12-bit (float) parts through the versioned `_Dunscale`, `_Dint` and `_Dscale`. `_Xp_ldexpx` and `_FXp_ldexpx` delegate to libc's `ldexp` and `ldexpf`; these versioned callers have not been requalified against that module after the scaling corrections. The versioned `_Feraise` only sets `errno`, and `_Xp_mulx` reads the second multiplier term even when it is given one term, as in the module.
