# libSceIcu

## add Functional

- [uprv_deleteUObject](../../../core/libs/prx/libSceIcu/LegacyExports.cpp) (libSceIcu) - not checked against `libSceIcu.sprx` or on a console. Forwards directly to ICU's `uprv_deleteUObject(object)` (`delete static_cast<icu::UObject*>(object)`), which assumes the host C++ virtual destructor ABI rather than invoking an explicit `APS5_VABI` deleting destructor slot.
