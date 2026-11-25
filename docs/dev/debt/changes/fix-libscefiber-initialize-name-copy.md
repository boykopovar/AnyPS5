# libSceFiber

## add Functional

- [sceFiberInitialize, _sceFiberInitializeWithInternalOptionImpl, sceFiberRename](../../../core/libs/prx/libSceFiber/Export.cpp) return SCE_FIBER_ERROR_INVALID when the name pointer is unreadable; initialize returns it before it reads or changes the fiber object. This is a defensive host-protection fallback; whether the console raises a signal or faults on unmapped memory, and where the console checks the name relative to the other arguments and the fiber state, is not measured.
