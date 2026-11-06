# libSceAudioLatencyEstimation technical debt

### Unknown function info

- [sceAudioLatencyEstimationAddData, sceAudioLatencyEstimationGetDelayTime, sceAudioLatencyEstimationInitialize and sceAudioLatencyEstimationRelease](../../../core/libs/prx/libSceAudioLatencyEstimation/Export.cpp) (libSceAudioLatencyEstimation) - names and NIDs are present in the [aerolib symbol database](https://github.com/zecoxao/sce_symbols/blob/master/aerolib.csv) and are attributed to this module by `sceAudioLatencyEstimation` symbol prefix; the library is absent from the PS5 3.20 export inventory, so the pairing is not firmware-verified and no credible prototypes were found. The declarations have no parameters solely to resolve imports; this is not an ABI claim and callers must not call them until signatures are verified
