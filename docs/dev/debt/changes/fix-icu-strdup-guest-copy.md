# libSceIcu

## add Functional

- [uprv_strdup and uprv_strndup sources](../../../core/libs/prx/libSceIcu/Configuration.cpp) (libSceIcu) - the source is read through checked copies, the terminator for `uprv_strdup` and `count` bytes for `uprv_strndup`; an unreadable source throws, as a null one does for `uprv_strdup`. ICU dereferences both, so this is a host-safety fallback with no console behaviour to match.
