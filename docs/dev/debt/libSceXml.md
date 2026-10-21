# libSceXml technical debt

### Unknown function info

- The 150 `sceXmlUnknown_*_nid_no_patch` exports in [Export.cpp](../../../core/libs/prx/libSceXml/Export.cpp) are present in the PS5 3.20 `libSceXml` export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Their NIDs are derived from the exported mangled symbols. Their prototypes and behavior are not established; each declaration uses the existing no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`. The declarations do not claim an ABI or behavior.
