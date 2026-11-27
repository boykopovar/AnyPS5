# libc

## add Unknown function info

- [__sync_fetch_and_add_16, __sync_fetch_and_sub_16, __sync_fetch_and_and_16, __sync_fetch_and_or_16, __sync_fetch_and_xor_16 and __sync_lock_test_and_set_16](../../../core/libs/prx/libc/src/AtomicBuiltins.cpp) (libc) - these six exports now validate that the target is non-null and 16-byte aligned before touching memory, and throw `std::invalid_argument` instead of dereferencing it. Console behavior for a null or misaligned target is unverified; the console's `lock cmpxchg16b` path is expected to fault (#GP or segfault) rather than return, so this runtime throws `std::invalid_argument` instead of faulting.
