# libSceAudioOut

## change [AudioOut2 main output]

- [AudioOut2 main output](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Context.cpp) (libSceAudioOut) - a context opens the host's default output by its name, not as SDL's default device. Before a push, a context whose device SDL reports as stopped, or whose default output's name no longer matches the host's default (checked at most once a second), closes it and opens the host's default of that moment; its mixed but unplayed audio stays queued for the new device. When that open fails, the context runs without a device for the rest of its life. The console's behaviour when its output disappears or changes is not measured
