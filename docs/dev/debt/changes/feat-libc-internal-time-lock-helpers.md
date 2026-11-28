# libSceLibcInternal

## add Functional

- [Spin lock and time zone helpers](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Lock_spin_lock` and `_Unlock_spin_lock` implement Dinkumware 1-byte atomic spinlock synchronization with pause spinning. `_Getzone` returns the default timezone separator string `":"`, `_Tzoff` returns 0 for UTC offset, and `_Gettime` parses colon-separated time string tokens and extracts field pointers and lengths according to Dinkumware libc specifications.
