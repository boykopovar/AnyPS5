# libSceAudioIn

## add Functional

- [openDevice](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - the SDL capture open runs on a helper thread and the caller waits 3 s for it without the port table lock. Meanwhile the slot is reserved and every AudioIn call, close included, answers it as not open (0x80260109); opens wait for each other. An open waiting behind a stalled open skips SDL while the earlier helper is still running. A device that does not open in time leaves the port without a device (silence), and a named device fails with 0x8026010f while SDL is skipped. The helper closes a device opened late before capture opens resume. The 3 s limit is a host choice, not a console value. A thread that never returns from SDL stays blocked until the process exits. The reservation test needs a FIFO, so only Linux runs it. SDL's PipeWire backend never returns from a capture open on a host whose only source is the monitor of an output (issue #26458).
