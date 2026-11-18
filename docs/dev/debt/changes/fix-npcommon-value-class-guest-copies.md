# libSceNpCommon

## add Functional

- [sce::np::Time comparisons and operator+, sce::np::NpTitleId, NpTitleSecret and NpCommId](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) (libSceNpCommon) - the comparison operators, `operator+`, the constructors, assignments, `Clear`, `IsEmpty`, `Equals` and `SetSecretData` copy the objects, ticks, ids and secret data they read in with `GuestCopyIn_nid_no_patch` and write their results with `GuestCopyOut_nid_no_patch`. An object or buffer that can't be read or written fails through `NotImplementedAt_nid_no_patch`, since these C++ members have no error result. `NpTitleId::Equals(const char*)` reads the text up to its first 12 characters or its terminator. This is a host-safety fallback; the console's behaviour for inaccessible objects is not measured. `ParseHex`, `BuildHex`, `Build` and `Parse(const char*, size_t)` keep their own paths.
