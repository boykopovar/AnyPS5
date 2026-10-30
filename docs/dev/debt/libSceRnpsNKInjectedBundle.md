# libSceRnpsNKInjectedBundle technical debt

### Unknown function info

- [WKBundleInitialize](../../../core/libs/prx/libSceRnpsNKInjectedBundle/Export.cpp) - name and NID match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceRnpsNKInjectedBundle.c); signatures and behavior are unverified. No-parameter declaration resolves imports and throws through `NotImplemented_nid_no_patch` when called.
