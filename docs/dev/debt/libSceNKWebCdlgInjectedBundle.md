# libSceNKWebCdlgInjectedBundle technical debt

### Known export names; ABI prototypes unverified

- [WKBundleInitialize_nid_disambig1](../../../core/libs/prx/libSceNKWebCdlgInjectedBundle/Export.cpp) (libSceNKWebCdlgInjectedBundle) - the one export of the PS5 3.20 inventory has an unverified prototype and behavior. Its name is the WebKit injected-bundle entry point whose `tools/nid_names.py` hash equals the inventory NID `es-hzEqlzr0`; a name match does not show that the console function has WebKit's prototype or behavior. It is declared as `WKBundleInitialize_nid_disambig1` because libSceRnpsNKInjectedBundle exports the same name; it uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`. Source: [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs).
