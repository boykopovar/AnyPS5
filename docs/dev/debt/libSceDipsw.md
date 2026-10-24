# libSceDipsw technical debt

### Silent stubs

- [sceKernelInitializeDipsw, sceKernelIsAssistMode, sceKernelIsDevelopmentMode](../../../core/libs/prx/libSceDipsw/Export.cpp) (libSceDipsw) - return 0 without consulting console state: the host emulates a retail console, so assist and development modes are off and there is no dipsw table to initialize. The declarations have no parameters solely to resolve known imports; the signatures are unverified host-emulation fallback assumptions, not measured console behaviour

### Unknown function info

- [sceKernelCheckDipsw, sceKernelIsDisableRazor, sceKernelReadDipswData, sceKernelSetDipsw, sceKernelUnsetDipsw and sceKernelWriteDipswData](../../../core/libs/prx/libSceDipsw/Export.cpp) (libSceDipsw) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceDipsw.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
