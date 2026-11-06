# libSceVideoStreamingEngineAudioOnly technical debt

### Unknown function info

- [4 exports (4 without a known name, bound by NID)](../../../core/libs/prx/libSceVideoStreamingEngineAudioOnly/Export.cpp) (libSceVideoStreamingEngineAudioOnly) - the function exports of `libSceVideoStreamingEngineAudioOnly.sprx` that no other library defines from the firmware 13.60 export table, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown; every export throws through `NotImplemented_nid_no_patch`
- [70 shared exports](../../../core/libs/prx/libSceVideoStreamingEngineAudioOnly/Export.cpp) (libSceVideoStreamingEngineAudioOnly) - the other 70 function exports of `libSceVideoStreamingEngineAudioOnly.sprx` have the NIDs of exports already in libSceVideoStreamingEngine, so they are not defined again here
