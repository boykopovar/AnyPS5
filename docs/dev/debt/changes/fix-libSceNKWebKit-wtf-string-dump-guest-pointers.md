# libSceNKWebKit

## add Functional

- [WtfStringDump guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/WtfStringDump.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WTF::printInternal(PrintStream&, const StringImpl*)` and `WTF::URL::dump(PrintStream&)` is unmeasured; both exports copy `WTF::URL`, `WTF::StringImpl` headers and character buffers, and `WTF::PrintStream` vtable pointers/slots through `GuestCopyIn_nid_no_patch`, calling `NotImplementedAt_nid_no_patch` on unmapped, unreadable, or null-slot guest memory. Closes when: console behavior for invalid guest pointers in `WtfStringDump` exports is verified on hardware.
