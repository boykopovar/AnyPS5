# libSceUlt

## add Functional

- [sceUltQueuePush, sceUltQueueTryPush, sceUltQueuePop and sceUltQueueTryPop](../../../core/libs/prx/libSceUlt/Export.cpp) (libSceUlt) - queue payload bytes are transferred through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` instead of dereferencing the guest-supplied pointer in host `memcpy`. An unreadable non-null input or unwritable non-null output returns the existing `ULT_ERROR_NULL` without publishing or consuming a queue entry, and preserves `errno`. The console's behavior for unmapped non-null queue payload pointers has not been measured; this is a host-safety fallback. A guest-copy operation may partially modify the destination before reporting failure, although the queue's length and ordering remain unchanged.
