# libSceNgs2.native

## add Functional

- [sceNgs2RackGetVoiceHandle and sceNgs2CustomRackGetModuleInfo guest pointers](../../../core/libs/prx/libSceNgs2.native/src/Rack.cpp) (libSceNgs2.native) - the voice handle and the module info are written through the guest copy helpers; returning `INVALID_OUT_ADDRESS` for an unwritable output (what each returns for a null one) is a host-safety fallback, and whether the console faults or returns an argument error on unmapped memory is unmeasured.
