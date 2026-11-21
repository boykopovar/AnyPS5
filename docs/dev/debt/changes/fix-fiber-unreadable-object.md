# libSceFiber

## add Functional

- [sceFiberFinalize, sceFiberRun, sceFiberSwitch, sceFiberGetInfo, sceFiberRename, _sceFiberInitializeImpl, _sceFiberInitializeWithInternalOptionImpl, _sceFiberAttachContextAndRun, _sceFiberAttachContextAndSwitch](../../../core/libs/prx/libSceFiber/Export.cpp) (libSceFiber) - reading a fiber object's magic through `GuestCopyIn_nid_no_patch`, and checking that the whole 0x100-byte object is writable before initializing it, so an unreadable object or an unwritable one passed to initialization returns `SCE_FIBER_ERROR_INVALID` with `errno` preserved, is a host-safety fallback; whether the console faults on those pointers is not measured.
