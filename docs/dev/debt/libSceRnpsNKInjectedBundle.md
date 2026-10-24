# libSceRnpsNKInjectedBundle technical debt

### Unknown function info

- `WKBundleInitialize` — NID `es-hzEqlzr0` — the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceRnpsNKInjectedBundle.c) confirms the named export. Its signature and behavior are unverified; the declaration in [Export.cpp](../../../core/libs/prx/libSceRnpsNKInjectedBundle/Export.cpp) uses the no-parameter unknown-signature convention and throws through `NotImplemented_nid_no_patch`, for import resolution only.
