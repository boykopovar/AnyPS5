# libSceNKWebKit

## add Functional

- [JscDumps guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/JscDumps.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `JSC::CellAttributes::dump` and `JSC::DOMJIT::HeapRange::dump` is unmeasured; both exports copy `self`, `WTF::PrintStream` vtable pointers, and vtable dispatch slots through `GuestCopyIn_nid_no_patch`, calling `NotImplementedAt_nid_no_patch` on unmapped, unreadable, or null-slot guest memory. Closes when: console behavior for invalid guest pointers in `JscDumps` exports is verified on hardware.
