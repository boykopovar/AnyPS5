# libSceNKWebCdlgInjectedBundle technical debt

### Known export names; ABI prototypes unverified

- [sceNKWebCdlgInjectedBundleUnknown_*_nid_no_patch](../../../core/libs/prx/libSceNKWebCdlgInjectedBundle/Export.cpp) (libSceNKWebCdlgInjectedBundle) - 1 PS5 3.20 import declarations (`WKBundleInitialize`). Their prototypes and behavior are unverified; each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`. This records the export names without claiming an ABI or behavior. Source: [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs).
