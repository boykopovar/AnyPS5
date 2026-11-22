# libc

## add Unknown function info

- [seed48 and lcong48 inputs](../../../core/libs/prx/libc/src/Random.cpp) (libc) - the seed words and the lcong48 parameters are copied in with `GuestCopyIn_nid_no_patch` before the rand48 state changes; an unreadable array throws the invalid-argument exception a null one already throws and leaves the state alone, because the console's behaviour on unmapped memory is unmeasured.
