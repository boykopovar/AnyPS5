# libSceNpManager

## add Destructors
- [NpTicket and value-class destructor guest copies](../../../core/libs/prx/libSceNpManager/Destructors.cpp) (libSceNpManager) - `NpTicket` destructors, `NpTicket::Clear`, and the five value-class deleting destructors read and write `TicketObject`, `NpAllocatorEx`, `NpAllocator`, and the 16-byte object header through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch`. Ignoring null `self`, underflowing addresses below 16 bytes, unreadable/unwritable guest pointers, null allocators, and null release callbacks without crashing is a host-safety fallback; console behaviour on invalid pointers is unmeasured, and host `errno` is preserved.
