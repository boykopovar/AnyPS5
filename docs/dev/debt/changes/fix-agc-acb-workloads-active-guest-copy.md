# libSceAgc

## add Functional

- [sceAgcAcbSetWorkloadsActive](../../../core/libs/prx/libSceAgc/Acb/src/Control.cpp) (libSceAgc) - for a registered stream, workload ids that cannot be read through `GuestCopyIn_nid_no_patch` return null without writing to the command buffer, the result for null ids. This is a host-safety fallback: the console's result for unreadable ids on a registered stream is not measured.
