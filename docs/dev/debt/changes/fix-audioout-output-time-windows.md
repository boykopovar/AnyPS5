# libSceAudioOut

## add Functional

- [sceAudioOutClose](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - waits at most 50 ms for the host device to close and then returns while the close finishes on a background thread, because the Windows audio stack can take about a second to stop a stream; the process waits for pending closes at shutdown, and a title that reopens the same host endpoint immediately may see the old stream still draining
