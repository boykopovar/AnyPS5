# libSceTtsEngine technical debt

### Unknown function info

- [11 exports (2 without a known name, bound by NID)](../../../core/libs/prx/libSceTtsEngine/Export.cpp) (libSceTtsEngine) - every function export of `libSceTtsEngine.sprx` from the firmware 13.60 export table, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown; every export throws through `NotImplemented_nid_no_patch`
