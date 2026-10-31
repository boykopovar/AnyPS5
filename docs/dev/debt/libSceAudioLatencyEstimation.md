# libSceAudioLatencyEstimation technical debt

### Silent stubs

- [sceAudioLatencyEstimationAddData](../../../core/libs/prx/libSceAudioLatencyEstimation/Export.cpp) (libSceAudioLatencyEstimation) - returns 0 when initialized without feeding hardware audio buffers.

### Functional

- [sceAudioLatencyEstimationInitialize, sceAudioLatencyEstimationRelease](../../../core/libs/prx/libSceAudioLatencyEstimation/Export.cpp) (libSceAudioLatencyEstimation) - manage atomic initialization state.

### Unknown function info

- [libSceAudioLatencyEstimation](../../../core/libs/prx/libSceAudioLatencyEstimation/Export.cpp) - names are in the [aerolib symbol database](https://github.com/zecoxao/sce_symbols/blob/master/aerolib.csv) and attributed by prefix; the module is absent from the 3.20 inventory, so pairing is unverified. Callers must not call declarations until signatures are verified.
