# libSceAudioOut

## add Functional

- [sceAudioOutClose, sceAudioOutOpen](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - on Windows every SDL open and close of an output device takes about 0.9 s on all SDL audio drivers measured (WASAPI, DirectSound, WinMM), so a closed main-path port parks its paused, flushed SDL device and the next open with the same device name and requested format reuses it; at most four are parked and the disk driver is never parked, so its files are complete when close returns. Parked devices are not closed until a newer one displaces them or the process ends. A parked device opened for the default output, or for a configured name that fell back to the default, is not re-resolved when it is reused, so if the host default output changed while it was parked it can keep playing on the previous endpoint unless the SDL driver follows default changes itself.
