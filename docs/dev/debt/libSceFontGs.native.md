# libSceFontGs.native technical debt

### Unknown function info

- [6 exports](../../../core/libs/prx/libSceFontGs.native/Export.cpp) (libSceFontGs.native) - every function export of `libSceFontGs.native.sprx` from the firmware 13.60 export table, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown; every export throws through `NotImplemented_nid_no_patch`
