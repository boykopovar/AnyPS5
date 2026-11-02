# libkernel_web technical debt

### Unknown function info

- 42 [libkernel_web exports](../../../core/libs/prx/libkernel_web/Export.cpp) carry the PS5 3.20 import names whose `tools/nid_names.py` hash equals their NID (`__freeze`, `__sceKernelGetGPI`, the `__sys_*` wrappers, `_execve`, `_execvpe`, `_wait4`, `sceKernelGetPrefixVersion`, `sceKernelIccGetThermalAlert`); `sceKernelWebUnknown_038_nid_no_patch` is `_dup2` under its literal NID alias. A name match does not establish their prototypes or behavior: each keeps the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`. Source: [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs).
