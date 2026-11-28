# libSceNKWebKit

## add Functional

- [StringBuilder::appendQuotedJSONString guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/StringBuilderJSON.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WTF::StringBuilder::appendQuotedJSONString` is unmeasured; the export validates writable `StringBuilder` and destination buffer storage under `GuestArena::HostWrite`, reads guest `StringBuilder`, `WTF::String`, `WTF::StringImpl`, and character buffers through `GuestReadable_nid_no_patch` / `GuestCopyIn_nid_no_patch`, writes results through `GuestCopyOut_nid_no_patch`, and calls `NotImplementedAt_nid_no_patch` on unmapped, unreadable, or unwritable guest memory. Closes when: console behavior for invalid guest pointers in `WTF::StringBuilder::appendQuotedJSONString` is verified on hardware.
