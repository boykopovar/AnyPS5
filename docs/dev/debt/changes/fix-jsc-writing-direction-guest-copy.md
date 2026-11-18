# libSceJsc

## add Functional

- [WTF::StringImpl::defaultWritingDirection](../../../core/libs/prx/libSceJsc/src/WtfStringSearch.cpp) (libSceJsc) writes its strong-directionality output with `GuestCopyOut_nid_no_patch`, so a non-null output that can't be written throws `std::invalid_argument`, as a null string does. A null output is still skipped. This is a host-safety fallback; the console's behaviour for an inaccessible output is not measured.
