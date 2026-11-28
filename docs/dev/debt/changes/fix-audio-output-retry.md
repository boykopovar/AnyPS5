# libSceAudioOut

## change [Host output reopen]

- [Host output reopen](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - the console's behaviour when a port's output disappears is not measured. Here a port whose host device SDL reports as stopped reopens its default output or controller sound card before its next output, a host policy with no console counterpart. Failed default-output reopens are retried on output calls at two-second intervals. A port whose initial open fails, or whose controller sound card reopen fails, stays virtual for the rest of its life
