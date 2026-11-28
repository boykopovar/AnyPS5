# libSceAcm

## add Functional

- [Unknown NID `9+f3tPdeyP0` ring guest access](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - the ring fields are now read with `GuestCopyIn_nid_no_patch` and the two updated 32-bit fields are written together with `GuestCopyOut_nid_no_patch`, preserving host `errno`. An unreadable or unwritable non-null guest ring returns null without faulting the host, a host-safety fallback. The firmware 13.42 disassembly checks only for a null argument before directly accessing the ring; its inaccessible-pointer behavior would fault, and this difference has not been measured on console.
