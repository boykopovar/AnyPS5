library: libSceJsc
section: Functional
action: add
---
- [Inspector::ContentSearchUtilities::lineEndings guest pointer validation](../../../core/libs/prx/libSceJsc/src/ContentSearchUtilities.cpp) (libSceJsc) - unreadable guest `WTF::String`, `WTF::StringImpl`, or character storage and unwritable `SizeVector` return storage or allocated backing storage are rejected via `NotImplementedAt_nid_no_patch` before mutating return storage (with allocation rollback if publishing fails) rather than faulting. Console fault behavior is unmeasured.
