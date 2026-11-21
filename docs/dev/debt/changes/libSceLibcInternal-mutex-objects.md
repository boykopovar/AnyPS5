# libSceLibcInternal

## remove `_ZNSt6_Mutex5_LockEv`

## remove `_ZNSt6_Mutex7_UnlockEv`

## remove `_ZNSt6_MutexC2Ev`

## remove `_ZNSt6_MutexD2Ev`

## remove [_ZNSt6_MutexD1Ev]

## remove [_ZNSt6_MutexC1Ev]

## add Functional

- [std::_Mutex](../../../core/libs/prx/libSceLibcInternal/MutexObject.cpp) (libSceLibcInternal) - console execution and newer versions are unverified. The one-pointer object follows SDK 10.00 `target/include/yvals.h:1918-1933`; its six wrappers follow `target/sce_module/libc.prx`, with names from `libc.prx.sym`: C1/C2 at `0xb8080`, D1/D2 at `0xb80b0` (exception termination at `0xb80d5`), lock at `0xb80e0`, unlock at `0xb80f0`. The wrappers inherit the existing `_Mtxinit`, `_Mtxdst`, `_Mtxlock`, `_Mtxunlock` and guest allocation behavior. Destruction catches escaping exceptions and invokes guest `std::terminate` explicitly; compiler-only `noexcept` relies on the open [personality gap](libc.md). Unreadable objects or mutex slots and unwritable constructor destinations throw defensively; a failed destination copy releases the allocation. Firmware fault behavior is unmeasured, and forged handles or concurrently changed object storage retain the low-level native preconditions.

# libc

## add Functional

- [C++ exception personality](../../../core/libs/prx/libc/src/exception/Personality.cpp) can unwind through a GCC `noexcept` region encoded by absence from the LSDA call-site table: an unmatched call site continues unwinding instead of terminating. Reproduced on Windows MinGW GCC 15.2 with a throwing application-heap callback in a guest DLL; no console measurement. Explicit guest-terminate handlers, such as the SDK-backed mutex destructor, avoid this gap for their own calls; the general personality behavior remains open.
