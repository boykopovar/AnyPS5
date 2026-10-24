# libSceFreeTypeOl technical debt

### Known export names; ABI prototypes unverified

- The 150 `sceFreeTypeOlUnknown_*_nid_no_patch` exports in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOl/Export.cpp) are present in the PS5 3.20 export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Their prototypes and behavior are unverified. Each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.
