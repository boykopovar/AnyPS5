# Host dialog and audio services

This document describes the current host implementations and the constraints for adding visible dialogs and Audio3D playback. It does not establish console ABI compatibility. Known assumptions and missing functionality are recorded in [Technical debt](TechnicalDebt.md).

## Error dialogs

[`libSceErrorDialog`](../../core/libs/prx/libSceErrorDialog/Export.cpp) keeps a process-wide state under a mutex. The current implementation validates an open request and logs its error code; it creates no window. [`GuestErrorDialog`](../../core/libs/tests/GuestErrorDialog.cpp) checks the lifecycle and argument errors.

| State | Value | Transitions |
|-------|-------|-------------|
| None | 0 | Initialize sets Initialized |
| Initialized | 1 | Open sets Running; Terminate sets None |
| Running | 2 | UpdateStatus or Close sets Finished; Terminate sets None |
| Finished | 3 | Open sets Running; Terminate sets None |

GetStatus returns the current state without advancing it. Initialize outside None returns `ALREADY_INITIALIZED`. Open outside Initialized or Finished, and Close outside Running, return `INVALID_STATE`.

The current Open parameter is 16 bytes:

| Offset | Size | Use |
|--------|------|-----|
| 0 | 4 | Signed size field; must equal 16 |
| 4 | 4 | Signed error code; logged as hexadecimal |
| 8 | 4 | Signed user id; logged |
| 12 | 4 | Unused |

Windows checks memory commitment, guard/no-access flags and region bounds. Linux checks only for a non-null pointer before reading the parameter.

[`libSceMsgDialog.native`](../../core/libs/prx/libSceMsgDialog.native/Export.cpp) has different current behavior: Open finishes immediately and GetResult reports the OK button. The error-dialog parameter above must not be reused as a message-dialog parameter.

## Host display requirements

A visible implementation must preserve the guest's ability to poll status and cancel a running dialog:

- Open copies the request and returns without waiting for dismissal. UpdateStatus does not block on a native modal window.
- The host UI owner presents the request and records dismissal. The dialog reaches Finished after dismissal or a successful guest Close.
- Close and Terminate release or cancel the displayed request. Completion from an earlier request cannot finish a reopened dialog.
- Display runs outside the state mutex. Host callbacks can update state without deadlocking guest queries.
- Presentation works before a VideoOut window exists and remains cancellable during process shutdown.
- Presentation failure is reported explicitly; it is not treated as user acknowledgment.

[SDL2 message boxes](https://wiki.libsdl.org/SDL2/SDL_ShowSimpleMessageBox) are modal and block their calling thread. SDL recommends the parent window's owning thread, or the main thread when there is no parent. They cannot simply replace the state assignment in Open or UpdateStatus. The current [VideoOut presentation loop](../../core/libs/prx/libSceVideoOut/src/VideoOutDriver.cpp) handles SDL events but has no general cancellable dialog request bridge.

Verification needs controlled presentation, dismissal, cancellation before display, cancellation while displayed, reopen after close, termination while running and display failure. A separate host smoke test must verify visible text and dismissal without changing the guest ABI tests into interactive CI tests.

## Audio3D ports

[`libSceAudio3d`](../../core/libs/prx/libSceAudio3d/Export.cpp) currently supports one timing port, id 0, for the system user `0xFF`. Open requires initialization, the 48 kHz rate code, a nonzero object limit and queue depth, granularity in multiples of 256 samples, advance-and-push buffer mode 2 and two beds. Unsupported modes throw; opening a second port returns `OUT_OF_RESOURCES`.

Advance reserves a queue entry. Push moves advanced entries into timed playback slots. Each slot lasts `granularity / 48000` seconds; synchronous Push waits only when the queue is full, until its oldest slot completes. GetQueueLevel discards elapsed slots and reports used and available entries. Close clears the port, and Terminate requires the port to be closed. This pacing supplies no samples and produces no sound.

Adding playback requires verified bed/object submission signatures, sample formats, channel layouts and attribute meanings before accepting payloads. Object reservation alone does not provide audio. The existing [AudioOut implementation](../../core/libs/prx/libSceAudioOut) is the first place to look for host output and shutdown handling; a separate audio-device stack is not needed unless that path cannot satisfy the contract.

Playback checks must verify submitted samples, queue capacity, close during synchronous playback and unsupported input behavior. Host timing tests alone cannot establish that sound is correct. Console ABI uncertainties remain in Technical debt until settled by a cited reference or observed title/hardware behavior.
