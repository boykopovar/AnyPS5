# libkernel_sys

## remove | `__sys_rdup` | `9Iem9Wk07xU` |

## add Functional

- [__sys_rdup](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) - `__sys_rdup` duplicates a descriptor for process 0 or the calling process via `dup`, matching `sceKernelRdup` and console `sys_rdup`; a process ID other than 0 or the caller throws through `NotImplemented_nid_no_patch`. Sets caller guest errno on failure.

# libkernel_web

## add Functional

- [__sys_rdup](../../../core/libs/prx/libkernel_web/Export.cpp) (libkernel_web) forwards to the libkernel_sys implementation, so its behavior, errno results and limits apply. Console behavior is unverified.
