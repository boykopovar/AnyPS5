# libSceMat2Enc

## add Unknown function info

- [4 exports (4 without a known name, bound by NID)](../../../core/libs/prx/libSceMat2Enc/Export.cpp) (libSceMat2Enc) - every function export of `libSceMat2Enc.sprx` from the firmware 13.60 export table, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown; every export throws through `NotImplemented_nid_no_patch`
