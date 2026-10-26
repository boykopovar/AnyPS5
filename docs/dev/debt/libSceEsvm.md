# libSceEsvm technical debt

### Unknown function info

- [sceEsvmAppAudioSyncCreateContext, sceEsvmAppAudioSyncFlush and sceEsvmSyncVideo](../../../core/libs/prx/libSceEsvm/Export.cpp) (libSceEsvm) - signatures and behavior are unverified; these parameterless declarations only resolve imports and throw through `NotImplemented_nid_no_patch`.
