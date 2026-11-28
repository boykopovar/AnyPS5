# libSceJsc

## add Functional

- [Inspector::ContentSearchUtilities::lineEndings guest pointer validation](../../../core/libs/prx/libSceJsc/src/ContentSearchUtilities.cpp) (libSceJsc) - unreadable guest `WTF::String`, `WTF::StringImpl`, or character storage and unwritable `SizeVector` return storage or allocated backing storage are rejected via `NotImplementedAt_nid_no_patch` before mutating return storage (using two-phase tail-then-buffer publication with rollback so failed publication cannot expose freed backing memory) rather than faulting. Console fault behavior is unmeasured. Closes when: console behavior on invalid guest pointers in this export is measured.
