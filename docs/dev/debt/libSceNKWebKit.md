# libSceNKWebKit technical debt

### Known export names; ABI prototypes unverified

- The 150 `sceNKWebKitUnknown_*_nid_no_patch` exports in [Export.cpp](../../../core/libs/prx/libSceNKWebKit/Export.cpp) are present in the PS5 3.20 `libSceNKWebKit` export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Their NIDs are derived from exported mangled symbols. Prototypes and behavior are unverified; each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`. These declarations make no ABI or behavior claim.
