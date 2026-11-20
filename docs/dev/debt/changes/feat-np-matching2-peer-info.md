# libSceNpMatching2

## change [The signaling calls before initialization]

- [The signaling calls before initialization](../../../core/libs/prx/libSceNpMatching2/Export.cpp) (libSceNpMatching2) - `sceNpMatching2SignalingGetConnectionInfo`, `GetConnectionInfoA`, `GetConnectionStatus`, `GetLocalNetInfo`, `AbortConnection`, `CancelPeerNetInfo`, `EnableManualUdpMode`, `EstablishConnection`, `GetPeerNetInfo` and `GetPeerNetInfoResult` return 0x80550E01 (signaling NOT_INITIALIZED) before `sceNpMatching2Initialize` and after `sceNpMatching2Terminate`; the four remaining stateful calls still throw once initialized: what they do then is unknown. Firmware 13.42's `libSceNpMatching2.sprx` (read from disassembly and run under a CPU emulator on x86-64, not on a console) tests the signaling byte 0x90040 first in each of them; only `sceNpMatching2Initialize` sets it (through 0xd0) and `sceNpMatching2Terminate` clears it (through 0x150), so it follows `matching2Initialized` here. A failure inside those internal routines, which can leave the two flags apart, is not modelled.

## add Silent stubs

- [sceNpMatching2SignalingGetPeerNetInfo and sceNpMatching2SignalingGetPeerNetInfoResult](../../../core/libs/prx/libSceNpMatching2/Export.cpp) (libSceNpMatching2) - return 0 after initialization, following [shadPS4 at 264d6b7](https://github.com/shadps4-emu/shadPS4/blob/264d6b71d8671a429ad2c9d1a281e18558fb9693/src/core/libraries/np/np_matching2/np_matching2.cpp#L1079-L1082) and its corresponding `GetPeerNetInfoResult` implementation. These no-ops' console behavior is unverified.
