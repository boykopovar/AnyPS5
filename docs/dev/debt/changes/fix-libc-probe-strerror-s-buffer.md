# libc

## add Unknown function info

- [strerror_s buffer](../../../core/libs/prx/libc/src/WideConversion.cpp) (libc) - the message, truncated with "..." as before, is built in a host buffer and copied out with one `GuestCopyOut_nid_no_patch`; an unwritable buffer is reported as the same runtime-constraint violation (EINVAL, handler called with a null pointer) a null or oversized buffer gets, which is a host-safety fallback; whether the console module faults on unmapped memory is unmeasured.
