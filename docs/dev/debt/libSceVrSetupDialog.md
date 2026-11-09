# libSceVrSetupDialog technical debt

### Silent stubs

- [libSceVrSetupDialog](../../../core/libs/prx/libSceVrSetupDialog/Export.cpp) - the VR setup dialog runs the common-dialog state machine but shows nothing and reports the user as having cancelled, as no headset is supported (see libSceHmd2). Initialization does not check the native process eligibility record, and the 46-byte setup payload remains opaque. Open validates the recovered 56-byte parameter header. GetResult writes one 32-bit result after initialization, including when returning the not-finished error; its error codes and validation order are recovered. Returning `0x8A7C0003` for an unwritable result pointer is a host-safety fallback with console behavior unmeasured.
