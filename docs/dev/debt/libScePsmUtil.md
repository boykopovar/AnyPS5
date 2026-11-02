# libScePsmUtil technical debt

### Functional

- [scePsmUtilInitialize, scePsmUtilFinalize](../../../core/libs/prx/libScePsmUtil/Export.cpp) (libScePsmUtil) - atomic initialization lifecycle. Double initialization and uninitialized finalization or query calls throw `std::logic_error`; initialized queries dispatch to `NotImplemented_nid_no_patch`.

### Unknown function info

- [scePsmUtilFinalize, scePsmUtilGetPsmIniFile, scePsmUtilInitialize and scePsmUtilRegisterCallbacks](../../../core/libs/prx/libScePsmUtil/Export.cpp) (libScePsmUtil) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsmUtil.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
