# libSceAudioOut

## change [Host output reopen]

- [Host output reopen](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - the console's behaviour when a port's output disappears or the system output changes is not measured. Here a port opens the host's default output by its name, not as SDL's default device, so SDL never moves an open port itself; before an output, at most once a second, a port opened on the default output whose name no longer matches the host's default closes and reopens on the new default, dropping its queued audio. A port whose host device SDL reports as stopped reopens the default output of that moment, or its controller sound card, before its next output. Both are host policies with no console counterpart. When a reopen fails, the port runs as a virtual port for the rest of its life and is not retried

## change [AudioOut2 main output]

- [AudioOut2 main output](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Context.cpp) (libSceAudioOut) - a context opens the host's default output by its name, not as SDL's default device. Before a push, a context whose device SDL reports as stopped, or whose default output's name no longer matches the host's default (checked at most once a second), closes it and opens the host's default of that moment; its mixed but unplayed audio stays queued for the new device. When that open fails, the context runs without a device for the rest of its life. The console's behaviour when its output disappears or changes is not measured
