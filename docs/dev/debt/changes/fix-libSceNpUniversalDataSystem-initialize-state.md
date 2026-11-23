# libSceNpUniversalDataSystem

## change [sceNpUniversalDataSystemInitialize]

- [sceNpUniversalDataSystemTerminate](../../../core/libs/prx/libSceNpUniversalDataSystem/src/System.cpp) (libSceNpUniversalDataSystem) - what the 13.42 module's `Terminate` returns, initialized or not, and whether a later `Initialize` succeeds again is unverified: the module run in a host harness on Windows, not on a console (https://github.com/boykopovar/AnyPS5/pull/12803#issuecomment-6517047831), covered only `Initialize`. Here it clears the state `Initialize` sets, as `sceNpUniversalDataSystemIntTerminate` does with its own, and returns 0 either way, as it did before that state existed.
