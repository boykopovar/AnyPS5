# libSceRnpsNKInjectedBundle technical debt

### Unknown function info

- `WKBundleInitialize` — NID `es-hzEqlzr0` — the local PS5 3.20 export inventory confirms the named export. Its signature and behavior are unverified; the declaration in [Export.cpp](../../../core/libs/prx/libSceRnpsNKInjectedBundle/Export.cpp) uses the no-parameter unknown-signature convention and throws through `NotImplemented_nid_no_patch`, for import resolution only.
