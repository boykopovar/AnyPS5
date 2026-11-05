# libSceFrontPanelDisplay technical debt

### Unknown function info

`libSceFrontPanelDisplay` is registered in `core/libs/prx/libSceSysmodule/ModuleTable.hpp` (`kModuleTable` ID `0x0000011b` (and `0x011b` in `kPublicModuleIds`)). The 3 export(s) in [Export.cpp](../../../core/libs/prx/libSceFrontPanelDisplay/Export.cpp) come from the per-library stub list in [claimore22/ps5rs](https://github.com/claimore22/ps5rs/blob/5f252a658ff470bcb2b6cd15205fd8ca64ae4477/data/stubs/by_library/libSceFrontPanelDisplay_nosubmission.txt) (`data/stubs/by_library/libSceFrontPanelDisplay_nosubmission.txt`) and the pinned [aerolib symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) (`tools/nid_names.py` `compute_nid` equals the listed NID for every entry). That repository does not document how the list was extracted and the module is absent from the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/tree/b2a77c09cd71c01f34c790cfbe98c751a13262ad), so completeness, parameter layouts, and behavior are unverified: every declaration takes no parameters in the import-resolution convention (no ABI claim) and throws through `NotImplemented_nid_no_patch`.

- [sceFrontPanelDisplayInitialize](../../../core/libs/prx/libSceFrontPanelDisplay/Export.cpp) (libSceFrontPanelDisplay) - unknown signature (NID dl0qd+E20ng)
- [sceFrontPanelDisplaySetOutputMode](../../../core/libs/prx/libSceFrontPanelDisplay/Export.cpp) (libSceFrontPanelDisplay) - unknown signature (NID 8KjtCpKz0GQ)
- [sceFrontPanelDisplayStickReadState](../../../core/libs/prx/libSceFrontPanelDisplay/Export.cpp) (libSceFrontPanelDisplay) - unknown signature (NID jgAyn4OgfSI)
