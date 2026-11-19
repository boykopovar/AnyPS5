# libSceVsePlugin technical debt

### Unknown function info

- [34 exports (34 without a known name, bound by NID)](../../../core/libs/prx/libSceVsePlugin/Export.cpp) (libSceVsePlugin) - every function export of `libSceVsePlugin.sprx` from the firmware 13.60 export table, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown; every export throws through `NotImplemented_nid_no_patch`
