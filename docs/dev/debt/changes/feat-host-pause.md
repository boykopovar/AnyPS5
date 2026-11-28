# libSceVideoOut

## add Functional

- [Host pause](../../../core/libs/prx/libSceVideoOut/src/HostPause.cpp) (libSceVideoOut) - `ANYPS5_PAUSE_ON_FOCUS_LOSS` follows `SDL_WINDOWEVENT_FOCUS_LOST`, `SDL_WINDOWEVENT_MINIMIZED` and `SDL_WINDOWEVENT_FOCUS_GAINED` of the display window only. A window that starts without focus is not paused until its first focus event, and a restore from minimize resumes only through the focus-gained event that follows it. The event order was not observed on a real X11, Wayland, Windows or macOS window system, only through injected events
- [Host pause](../../../core/libs/prx/libSceVideoOut/src/VideoOutDriver.cpp) (libSceVideoOut) - the vblank counter keeps running during a pause, so the flips queued meanwhile (at most `VIDEO_OUT_FLIP_QUEUE_CAPACITY`) find their vblank target already passed and present back to back on resume instead of one per vblank; the effect on a title's frame pacing is not measured
- [Host pause](../../../core/libs/prx/libSceVideoOut/src/VideoOutDriver.cpp) (libSceVideoOut) - a pause holds the flip queue, it does not suspend the title: its threads keep running, vblank events keep arriving, and only a loop that waits on its flips or on a GPU label released by one stops, once the queue is full. A simulation or loading thread that does not wait on a flip keeps working while paused. Suspending guest threads (`pthread_suspend_np` style) is not used because a suspended thread can hold a guest lock; whether any title pauses itself differently on the console is not measured
- [Host pause](../../../core/libs/prx/libSceVideoOut/src/VideoOutDriver.cpp) (libSceVideoOut) - the guest-visible clocks are not held during a pause (`ANYPS5_PAUSE_KEY`, `ANYPS5_PAUSE_ON_FOCUS_LOSS`), so a title that measures elapsed time sees the whole pause as one frame on resume. A time offset applied in `sceKernelGetProcessTime`, `sceKernelGetProcessTimeCounter` and `sceKernelReadTsc` would leave a title's own `rdtsc` instruction, `sceKernelGetTscFrequency` and, on Linux, `sceKernelClockGettime` unshifted (the same limits as `APS5_TIME_SCALE` in [BUILD.md](../BUILD.md)), and sleeps and timed waits take real time, so a partial offset would make the clocks disagree with each other. Closes when: a title is observed to misbehave after a pause and a source for an offset that every clock honours exists

# libSceAudioOut

## add Functional

- [Host pause](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - a pause (`ANYPS5_PAUSE_KEY`, `ANYPS5_PAUSE_ON_FOCUS_LOSS`) queues silence for every non-vibration `sceAudioOutOutput` port and zeroes the AudioOut2 main mix in its device callback, so the title's audio threads keep their pacing and its music position keeps advancing silently. The vibration ports, the controller's own sound device (`QueuePadGrain` in `AudioOut2Context.cpp`) and the microphone are not silenced. The AudioOut2 callback change is not exercised by a test: the callback is static to the library and no headless test drives a real device

# libSceSystemService

## add Functional

- [Host pause](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - a pause (`ANYPS5_PAUSE_KEY`, `ANYPS5_PAUSE_ON_FOCUS_LOSS`) sends the title no system event: `sceSystemServiceReceiveEvent` still returns the no-event error. The console's event for a suspended and resumed title, and whether a title sees one when it is not put to rest mode, are not measured, so none is invented
