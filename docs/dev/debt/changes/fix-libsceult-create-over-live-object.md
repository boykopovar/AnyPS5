# libSceUlt

## change [libSceUlt create exports]

- [libSceUlt create exports](../../../core/libs/prx/libSceUlt/Export.cpp) (libSceUlt) - the mutex, condition variable, semaphore, reader-writer lock, queue, both resource pools, runtime and ulthread create exports copy `name` and clear the object through `CopyGuestString_nid_no_patch`, `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch`, and return `ULT_ERROR_NULL` without recording the object when the name is unreadable or the object is unwritable. A name is still truncated to 31 bytes. That answer is a host-safety fallback: what the console returns for a name or object on unmapped memory is unmeasured. After its argument checks, every create export returns `ULT_ERROR_STATE` for an address that is already registered, without clearing the object; that follows the library's own rule and is unmeasured on a console.
