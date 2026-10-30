# libSceXml technical debt

### Known export names; ABI prototypes unverified

- The 150 `sce::Xml` exports in [Export.cpp](../../../core/libs/prx/libSceXml/Export.cpp) are present in the PS5 3.20 `libSceXml` export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Each is named after the mangled symbol the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for its NID, and the NID of that name matches. Their prototypes and behavior are not established; each declaration uses the existing no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`. The declarations do not claim an ABI or behavior.
