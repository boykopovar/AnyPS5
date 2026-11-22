# libSceLibcInternal

## change `_Ttotm` (NID `4TTbo2SxxvA`)

- [Additional 3.20 imports](../../../core/libs/prx/libSceLibcInternal/InventoryExports2.cpp) (libSceLibcInternal) - `_Tzoff` (NID `Csx50UU9b18`); signatures and behavior are unverified and calls throw through `NotImplemented_nid_no_patch`.

## add Functional

- [_Ttotm](../../../core/libs/prx/libSceLibcInternal/InventoryExports2.cpp) (libSceLibcInternal) follows a [reading of firmware 13.42's libSceLibcInternal.sprx](https://github.com/boykopovar/AnyPS5/issues/21874#issuecomment-6946707639), read from disassembly and run natively, not measured on a console. `struct tm* _Ttotm(struct tm* t, time_t secs, int isdst)` adds 2208988800 to `secs` as an unsigned 64-bit value and fills `tm_sec` to `tm_yday` with Dinkumware's year loop, whose leap test is `year & 3`: 1900 is common and 2100 counts as a leap year. It stores `isdst` unchanged and returns `t`. Times before 1900 wrap into large years, as on the console. A null `t` uses a per-thread structure, as the disassembly shows; that path was not run there. An unwritable `t` throws, where the console faults.
