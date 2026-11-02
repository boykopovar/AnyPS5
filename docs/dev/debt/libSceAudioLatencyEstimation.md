# libSceAudioLatencyEstimation technical debt

### Functional

- [sceAudioLatencyEstimationInitialize, sceAudioLatencyEstimationRelease](../../../core/libs/prx/libSceAudioLatencyEstimation/Export.cpp) (libSceAudioLatencyEstimation) - module lifecycle tracks initialization state atomically; double initialization and uninitialized release throw `std::logic_error`. Whether hardware uses error codes is unmeasured.

### Unknown function info

- [sceAudioLatencyEstimationAddData, sceAudioLatencyEstimationGetDelayTime](../../../core/libs/prx/libSceAudioLatencyEstimation/Export.cpp) (libSceAudioLatencyEstimation) - names and NIDs are from the [aerolib symbol database](https://github.com/zecoxao/sce_symbols/blob/master/aerolib.csv) and attributed by prefix; absent from the PS5 3.20 inventory. Parameterless declarations resolve imports; uninitialized calls throw `std::logic_error` and initialized paths throw `NotImplemented_nid_no_patch`.
