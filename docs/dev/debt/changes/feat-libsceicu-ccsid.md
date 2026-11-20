# libSceIcu

## add Functional

- [CCSID converter opener](../../../core/libs/prx/libSceIcu/ConverterCcsid.cpp) (libSceIcu) - `ucnv_openCCSID` forwards to the host ICU entry point with the guest status word copied in and out, passing the platform straight through (ICU does not validate it; UCNV_UNKNOWN is -1 and UCNV_IBM is 0) and honouring an incoming failure instead of overwriting it. The conversion a CCSID names is resolved by ICU's converter alias data, which this build does not ship, so the test covers the deterministic paths only: the -1 and 0 platforms, a null status and an incoming error. `ucnv_openCCSID` therefore answers `nullptr` with whatever status ICU reports for a missing alias table.
