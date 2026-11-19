# libkernel_web technical debt

### Unknown function info

- 43 [libkernel_web exports](../../../core/libs/prx/libkernel_web/Export.cpp) carry the PS5 3.20 import names whose `tools/nid_names.py` hash equals their NID (`__freeze`, `__sceKernelGetGPI`, the `__sys_*` wrappers, `_dup2`, `_execve`, `_execvpe`, `_wait4`, `sceKernelGetPrefixVersion`, `sceKernelIccGetThermalAlert`). A name match does not establish their prototypes or behavior: the other 40 keep the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`. Source: [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs).

### Functional

- [`_dup2`, `_execve` and `_execvpe`](../../../core/libs/prx/libkernel_web/Export.cpp) are unverified on a console. They assume the prototypes and behavior of the libkernel_sys exports of the same names and forward to those implementations, so their errno results and the limits in the [libkernel_sys debt](libkernel_sys.md) apply. Whether the console's libkernel_web variants differ from libkernel_sys is not known. Closes when: compared with the console.

- The [generated forwarders](../../../core/libs/prx/libkernel_web/Forwards.cpp) assume each libkernel_web export behaves exactly like the same-named libkernel or libkernel_sys export. The 4.03 firmware dump supports this: every function body shared between libkernel_web and libkernel is instruction-identical once PLT/GOT and RIP-relative layout differences are normalized, so web is a pure mirror of the base kernel in that firmware. Prototypes come from our own provider declarations, not from the console. Whether a web variant is allowed to diverge (for example sandboxed syscall limits) is not known. Closes when: compared with the console.

- `errno` (NID `8KHh+VefZDM`) is a 4-byte per-module object in firmware, but our errno lives in host TLS behind `__error`. Exporting a static cell in [Export.cpp](../../../core/libs/prx/libkernel_web/Export.cpp) would give guest code storage disconnected from the value our host code sets, and COFF exports cannot forward to another module's storage, so the export is missing rather than wrong. Closes when: a shared-storage mechanism exists.

- `AxgW94umKqU` is an unnamed 8-byte object export in libkernel_web (and at least one other module). No provider or name exists anywhere in our sources, so its semantics are unknown and [Export.cpp](../../../core/libs/prx/libkernel_web/Export.cpp) exports nothing for it. Closes when: the object is identified.

- [`__stack_chk_guard` and `_sigintr`](../../../core/libs/prx/libkernel_web/Export.cpp) are mirrored as web-local objects. `_sigintr` is a value copy: a guest writing it directly updates only the web cell while our `siginterrupt`/`sigaction` implementations use libkernel's cell, and firmware's guard object is 64 bytes where ours exports a single `uintptr_t`. Closes when: console evidence shows a divergence that matters.

- [`__progname` and `environ`](../../../core/libs/prx/libkernel_web/Export.cpp) are not mirrored: COFF exports cannot forward to another module's storage, and a web-local copy would go stale when `setprogname` or environment updates replace the provider's pointer. Same missing-rather-than-wrong rationale as `errno`. Closes when: a shared-storage mechanism exists.

- [`getcontext`, `swapcontext` and `sigsetjmp`](../../../core/libs/prx/libkernel_web/Export.cpp) are not mirrored: their libkernel providers are asm routines that capture the caller's return address and stack pointer, so a C++ forwarder frame would hand `setcontext`/`siglongjmp` a dead frame. They need an asm tail-jump thunk per mirror rather than a plain forwarder. Closes when: such thunks exist or the firmware shows the exports unreachable.
