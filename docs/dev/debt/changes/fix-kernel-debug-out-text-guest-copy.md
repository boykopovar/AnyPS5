# libkernel

## add Functional

- [sceKernelDebugOutText](../../../core/libs/prx/libkernel/System/src/Notification.cpp) (libkernel) copies its text in with `CopyGuestString_nid_no_patch` before writing it, so an unreadable text returns `0x8002000e`, as a null one does. Text longer than 64 KiB is cut to its first 64 KiB. This is a host-safety fallback: the console's result for inaccessible or very long text is not measured. Closes when: compared with the console.
