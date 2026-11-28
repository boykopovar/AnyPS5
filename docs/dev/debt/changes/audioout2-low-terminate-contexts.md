# libSceAudioOut

## change [sceAudioOut2LoTerminate]

- [sceAudioOut2LoTerminate](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Low.cpp) (libSceAudioOut) - returns 0 whether or not the low-latency service runs, as the PS5 13.60 module does; it now destroys all host AudioOut2 contexts and their ports, matching the module's clearing of its audio-service tables. The host has no audio-server connection to close.
