# libSceNKWebKit

## add Functional

- [ExceptionData guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/ExceptionData.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::ExceptionData::isolatedCopy` is unmeasured; the export validates guest `ExceptionData`, nested `WTF::StringImpl`, and character buffers with checked guest reads and calls `NotImplementedAt_nid_no_patch` on unmapped or unreadable guest memory. Closes when: console behavior for invalid guest pointers in `WebCore::ExceptionData::isolatedCopy` is verified on hardware.
