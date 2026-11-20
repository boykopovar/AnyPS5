# libkernel_sys

## remove `2xg4JXvaJ6k`

## add Functional

- [Unnamed module code-span export `2xg4JXvaJ6k`](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) returns 1 only when the address lies in the executable segment of the loaded module identified by the handle; an unknown handle or any other address returns 0. This follows the firmware 13.42 [disassembly reading](https://github.com/boykopovar/AnyPS5/issues/21866#issuecomment-6951229328), which was not measured on a console. The host module-info lookup substitutes for the firmware's locked module list; concurrent unload behavior remains unmeasured.

## change [libkernel_sys additional exports]

- [libkernel_sys additional exports](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) - 298 callable exports have import-resolution declarations only. Parameter lists, return types and behavior remain unverified. Parameterless `int` declarations are placeholders, not verified ABI prototypes; every entry throws through `NotImplemented_nid_no_patch`. Unresolved names use literal NID aliases; no runtime functionality or data objects are fabricated. Existing providers and exports supplied by open pull requests are excluded. Kernel variant imports already resolve through `libkernel`, then `libc`; existing providers remain unchanged. Six native data exports (`errno`, `__stack_chk_guard`, `_sigintr`, `__progname`, `environ`, and NID `AxgW94umKqU`) require separate storage, layout, TLS and ownership validation and are not added as callable aliases or fabricated globals.
