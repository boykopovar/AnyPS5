# libc

## change [_Mtx_current_owns]

- [_Mtx_current_owns](../../../core/libs/prx/libc/src/Threads.cpp) (libSceLibcInternal, implemented in libc, NID `VYQwFs4CC4Y`) returns 1 and ignores its argument. This follows the PS5 12.00 body reading reported in [PR8859](https://github.com/boykopovar/AnyPS5/pull/8859), rather than inspecting host mutex ownership. A payload process on PS5 firmware 11.60 measured 1 as well in six calls: before locking, locked by the caller, after unlocking, held by another thread, for a zeroed unused slot and after `_Mtx_destroy`. The 12.00 observation is attributed to that contributor; other firmware versions and the argument's console type remain unverified.
