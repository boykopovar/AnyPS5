# libkernel_web technical debt

### Unknown function info

- 43 [libkernel_web exports](../../../core/libs/prx/libkernel_web/Export.cpp) carry the PS5 3.20 import names whose `tools/nid_names.py` hash equals their NID (`__freeze`, `__sceKernelGetGPI`, the `__sys_*` wrappers, `_dup2`, `_execve`, `_execvpe`, `_wait4`, `sceKernelGetPrefixVersion`, `sceKernelIccGetThermalAlert`). A name match does not establish their prototypes or behavior: the other 40 keep the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`. Source: [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs).

### Functional

- [`_dup2`, `_execve` and `_execvpe`](../../../core/libs/prx/libkernel_web/Export.cpp) are unverified on a console. They assume the prototypes and behavior of the libkernel_sys exports of the same names and forward to those implementations, so their errno results and the limits in the [libkernel_sys debt](libkernel_sys.md) apply. Whether the console's libkernel_web variants differ from libkernel_sys is not known. Closes when: compared with the console.
