# libSceAudioOut

## change [Host output reopen]

- [Host output reopen](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - the console's behaviour when a port's output disappears is not measured. Here a port opens the host's default output by its name, not as SDL's default device, so SDL never moves an open port to a new default device; a port whose host device SDL reports as stopped reopens the default output of that moment, or its controller sound card, before its next output, a host policy with no console counterpart. A port therefore stays on the device it opened while that device exists, even when the host's default output changes. When that reopen fails, the port runs as a virtual port for the rest of its life and is not retried
