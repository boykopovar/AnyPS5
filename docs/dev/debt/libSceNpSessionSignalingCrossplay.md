# libSceNpSessionSignalingCrossplay technical debt

### Unknown function info

- [11 exports (6 without a known name, bound by NID)](../../../core/libs/prx/libSceNpSessionSignalingCrossplay/Export.cpp) (libSceNpSessionSignalingCrossplay) - every function export of `libSceNpSessionSignalingCrossplay.sprx` from the firmware 13.60 export table, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown apart from the checks under Functional; past those every export throws through `NotImplemented_nid_no_patch`

### Functional

- [sceNpSessionSignalingCrossplayInitialize, sceNpSessionSignalingCrossplayGetMemoryInfo and sceNpSessionSignalingCrossplayUnknown02 (`wypjyjGWaLM`)](../../../core/libs/prx/libSceNpSessionSignalingCrossplay/Export.cpp) (libSceNpSessionSignalingCrossplay) - only the null checks firmware 13.42 makes before its first call are implemented, from its disassembly, not measured on a console: each returns `0x80553D03` for a null first argument (Initialize, GetMemoryInfo) or a null second, fourth or fifth argument (Unknown02). Past them they throw. Only the pointers these checks read are declared, with Unknown02's first and third arguments as 64-bit placeholders that keep the later ones in place; their types and names are not from a header.
