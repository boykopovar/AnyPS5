# libSceRemoteplay

## add Functional

- [37 pre-initialization gates](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - only the answers before initialization and after termination follow firmware 13.60 `libSceRemoteplay.sprx` (SHA-256 `f54acfb49cd605c50cdd01e94ee2dd06cd0ce466e48d70b78a9cfd5fe478bda9`), read from disassembly and not measured on a console: 36 exports (`IsRemoteViewering`, `NotifyCanWakeup`, `RegistCallback`, `UnregistCallback`, `GetClientInfo`, `DisableInputDevice`/`EnableInputDevice`, `HV7jZe1frbM`, and 28 `Api.hpp` entries from `Approve` to `SetRpMode`) return `0x80FC0004`, and `sceRemoteplayIsRemotePlaying` (`0x21f0`) returns `0`; once initialized they still throw. The 10 exports checking arguments before state remain throwing.
