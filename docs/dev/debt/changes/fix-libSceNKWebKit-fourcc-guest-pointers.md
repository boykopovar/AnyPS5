# libSceNKWebKit

## add Functional

- [FourCC guest output pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/FourCC.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest output pointers in `WebCore::FourCC::fromString` is unmeasured; the export validates guest `OptionalFourCC` output storage for readability and writability, publishes results through `GuestCopyOut_nid_no_patch` with rollback on failure, and calls `NotImplementedAt_nid_no_patch` on unmapped or unwritable output memory. Closes when: console behavior for invalid guest pointers in `WebCore::FourCC::fromString` is verified on hardware.
