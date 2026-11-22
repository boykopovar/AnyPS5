# libSceAgc

## change [sceAgcSetSubmitMode]

- [sceAgcSetSubmitMode](../../../core/libs/prx/libSceAgc/Misc/src/Suspend.cpp) (libSceAgc) - only mode 0 is accepted and returns 0; any nonzero mode throws `invalid submit mode` through `Agc::Command::Require`, because the driver flag it toggles is not traced. In the SDK 9.04 modules (module parameter `0x09040001`, from a title's `fakelib`), `libSceAgc` forwards to the `libSceAgcDriver` export `oFb2hMcoJa4`, which returns `0x8a6d0005` while a driver state word is nonzero and otherwise sets bit `0x20000` in two submit flag words for any nonzero mode and clears it for 0 before returning 0. What that bit changes in a submit, and when the driver state word is nonzero, are not measured on a console, so no mode is persisted here. Closes when: the driver state handling and the submit-flag effect are measured on a PS5.
