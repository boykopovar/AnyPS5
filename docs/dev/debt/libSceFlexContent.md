# libSceFlexContent technical debt

### Unknown function info

- [12 exports (5 without a known name, bound by NID)](../../../core/libs/prx/libSceFlexContent/Export.cpp) (libSceFlexContent) - every function export of `libSceFlexContent.sprx` from the firmware 13.60 export table, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown; every export throws through `NotImplemented_nid_no_patch`
