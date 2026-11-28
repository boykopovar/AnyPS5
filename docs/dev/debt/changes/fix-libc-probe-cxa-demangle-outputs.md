# libc

## add Unknown function info

- [__cxa_demangle guest pointers](../../../core/libs/prx/libc/src/CxxAbiSupport.cpp) (libc) - the supplied length is copied in, and the demangled name, the length and the status are copied out with checked guest copies in the same order as before; an unreadable length or an unwritable buffer or length returns null with status -3 (the ABI's invalid-argument status, which it already gives for a null name or a buffer without a length), freeing a buffer it allocated itself, and an unwritable status is skipped. These are host-safety fallbacks; whether the console module faults on unmapped memory is unmeasured.
