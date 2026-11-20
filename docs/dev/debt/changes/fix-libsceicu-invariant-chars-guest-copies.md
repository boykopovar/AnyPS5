# libSceIcu

## add Functional

- [Invariant character string reads](../../../core/libs/prx/libSceIcu/InvariantChars.cpp) (libSceIcu) - `uprv_stricmp`, `uprv_strnicmp`, `uprv_isInvariantString` and `uprv_isInvariantUString` read their strings through checked copies in page-bounded chunks, only as far as ICU reads them: up to the first difference, terminator, variant character or the count. An unreadable string throws `std::invalid_argument` where ICU dereferences it, so this is a host-safety fallback with no console behaviour to match.
