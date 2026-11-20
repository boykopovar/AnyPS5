# libSceIcu

## add Functional

- [CCSID converter opener](../../../core/libs/prx/libSceIcu/ConverterCcsid.cpp) (libSceIcu) - `ucnv_openCCSID` forwards to the host ICU entry point with the guest status word copied in and out, rejecting a negative or out-of-range platform with `U_ILLEGAL_ARGUMENT_ERROR` and honouring an incoming failure instead of overwriting it. The conversion a CCSID names is resolved by ICU's converter alias data, which this build does not ship, so the test covers the deterministic paths only: a bad platform, a negative platform, a null status and an incoming error. `ucnv_openCCSID` therefore answers `nullptr` with whatever status ICU reports for a missing alias table.
