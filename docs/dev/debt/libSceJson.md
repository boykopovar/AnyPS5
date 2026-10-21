# libSceJson technical debt

### Unknown PS5 3.20 export info

- 12 pending exports in [Export.cpp](../../../core/libs/prx/libSceJson/Export.cpp) are present in the [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs) inventory. Their prototypes and behavior are unverified. They use the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.
