# libSceAudioIn

## add Functional

- [openDevice](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - the SDL capture open runs on a helper thread and the caller waits 3 s for it without holding the port table lock (the slot is reserved meanwhile but every AudioIn call treats it as not open until the open finishes, and opens wait for each other); a device that does not open in that time leaves the port without a device (silence, as a default device that cannot be opened), and every later open of the session skips SDL. The 3 s limit is a host choice, not a console value. A thread that never returns from SDL stays blocked until the process exits, and a device it opens late is closed. SDL's PipeWire backend never returns from a capture open on a host whose only source is the monitor of an output (issue #26458).
