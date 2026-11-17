# libSceAudioIn

## add Functional

- [Host input reopen](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - the console's behaviour when a port's input device disappears or the system input changes is not measured. Here a port without `ANYPS5_AUDIO_IN_DEVICE` opens the host's default input by its name, not as SDL's default device, so SDL never moves an open port itself; before a capture, a port whose device SDL reports as stopped, or whose default input's name no longer matches the host's default (checked at most once a second), closes it and opens the host's default of that moment, a host policy with no console counterpart. When that open fails, the port captures silence for the rest of its life
