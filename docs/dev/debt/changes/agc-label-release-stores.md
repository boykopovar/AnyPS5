# libSceAgcDriver

## change [Label access width]

- [Label access width](../../../core/libs/prx/libSceAgcDriver/Execution/include/Pm4.hpp) (libSceAgcDriver) - a 4-byte wait on half of a label stored as 8 bytes, or an 8-byte wait on a qword stored as two 4-byte halves, is a mixed-size atomic access that C++ does not order; x86-64 and arm64 see each aligned access whole, and ThreadSanitizer does not report it. `Pm4::LoadLabel` masks the address to the wait's width, as the GPU does. An 8-byte label is one release store: `Validate` rejects a misaligned 64-bit RELEASE_MEM and `AgcDriverStoreGuestLabel_nid_postfix` checks alignment. Only a two-dword write to a 4-byte-aligned address is two 4-byte release stores, low dword first, as the GPU writes it, so a title CPU read of those 8 bytes between the two stores sees one dword new. Closes when: each label is written and polled at one width, or a wait that overlaps a wider or narrower store reads it at the store's width.
