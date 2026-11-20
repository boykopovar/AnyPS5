# libkernel_sys

## change [`__sys_is_development_mode`]

- [`__sys_is_development_mode`](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) fixes development mode to disabled (`0`); development-console state and features are not modelled. On a PS5 payload process (firmware 11.60) it returned 0 in 100 calls and with three bogus arguments; nonzero mode bits and titles are unmeasured. The parameterless `uint64_t` declaration follows [PS5SDK](https://github.com/PS5Dev/PS5SDK/blob/a2e03a2a0231a3a3397fa6cd087a01ca6d04f273/ps5/libkernel.h#L68), not a verified console ABI. [kstuff-lite](https://github.com/EchoStretch/kstuff-lite/blob/aada7512a3ae0e5f9a556b1a38daef2aa0a9a19b/lib/syscalls.txt#L415) maps the export name to the query; [ps5-hen](https://github.com/cragson/ps5-hen/blob/1d7787b45fa9121d1e0877a5c0bf436b439d988b/hen/src/hook.cpp#L249-L252) manufactures `0xc001` in a hook and supplies no measurement of the original result.

## change [sceKernelGetKqueueFromEqueue]

- [sceKernelGetKqueueFromEqueue](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - the descriptor is written with `GuestCopyOut`; an unwritable `kq` returns `EINVAL` like a null one and preserves `errno`. Firmware 13.42 writes through the pointer, so this is a host-safety fallback: on a PS5 payload process (firmware 11.60) a null `kq` returned `0x80020016` with `errno` untouched and the pointer 0x1 raised `SIGSEGV`; titles not measured.
