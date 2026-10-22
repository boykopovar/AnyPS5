# libSceAbstractLocal technical debt

### Known export names; ABI prototypes unverified

- 70 pending exports in [Export.cpp](../../../core/libs/prx/libSceAbstractLocal/Export.cpp) are present in the [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs) inventory. Their prototypes and behavior are unverified. They use the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.
