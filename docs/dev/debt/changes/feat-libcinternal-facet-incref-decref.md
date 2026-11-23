# libSceLibcInternal

## remove _ZNSt6locale5facet7_DecrefEv

## remove _ZNSt6locale5facet7_IncrefEv

## add Functional

- [std::locale::facet::_Incref and _Decref](../../../core/libs/prx/libSceLibcInternal/FacetReferences.cpp) (libSceLibcInternal, NIDs `-EgSegeAKl4`, `AgxEl+HeWRQ`) follow Microsoft STL's `locale::facet` and are not verified on a console: `_Incref` atomically adds one to the facet's 32-bit reference count at offset 0x08, and `_Decref` atomically subtracts one and returns the facet when the count reaches zero, null otherwise. The count's place and width are the ones the facet vtables' retain and release entries in libc already use. A null facet throws.
