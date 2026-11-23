# libkernel_sys

## change [pthread_attr_setstackaddr and pthread_attr_getsolosched_np]

- [pthread_attr_setstackaddr and pthread_attr_getsolosched_np](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - firmware 13.60 `libkernel_sys.sprx` (SHA-256 `60921322b155ba649cfa29ea5e6d358ca0229424b4f73fd5c30ae7f7ce7c304c`) places `pthread_attr_setstackaddr` (NID `suCrEbr0xIQ`) at function address `0x4ed0` and `pthread_attr_getsolosched_np` (NID `ApMLfJ527Ag`) at `0x51b0`. `pthread_attr_setstackaddr` reads `attr` through `GuestCopyIn_nid_no_patch` with caller `errno` preserved and returns `EINVAL` (22) when `attr` is unreadable, matching its null-pointer return as a host-safety fallback. Console runtime behaviour was not independently tested
