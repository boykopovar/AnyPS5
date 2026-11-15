# libc

## add Unknown function info

- [mbrtoc32 / mbrtoc16 guest memory](../../../core/libs/prx/libc/src/Multibyte.cpp) (libc) - the conversion state is copied in and back out with `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch`, the source is read one byte at a time the same way, and the character or code unit is written with `GuestCopyOut_nid_no_patch`; an unreadable source or unwritable destination returns `(size_t)-1` with `EFAULT` and leaves the state unchanged, and an unreadable or unwritable state throws the invalid-argument exception a null state already throws, because their behaviour on unmapped memory is unmeasured.
