# libScePlayReady4L technical debt

### Unknown function info

- [8 exports (8 without a known name, bound by NID)](../../../core/libs/prx/libScePlayReady4L/Export.cpp) (libScePlayReady4L) - the function exports of `libScePlayReady4L.sprx` that no other library defines from the firmware 13.60 export table, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown; every export throws through `NotImplemented_nid_no_patch`
- [88 shared exports](../../../core/libs/prx/libScePlayReady4L/Export.cpp) (libScePlayReady4L) - the other 88 function exports of `libScePlayReady4L.sprx` have the NIDs of exports already in libScePlayReady, libScePlayReady2, libScePlayReady4, so they are not defined again here
