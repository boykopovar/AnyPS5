# libSceGameLiveStreaming technical debt

### Unknown function info

- [sceGameLiveStreamingGetCurrentStatus2](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - the status layout (user id, on-air flag, spectator, text and command message counts, broadcast resolution, 48 reserved bytes) is taken from shadPS4's `OrbisGameLiveStreamingStatus2`; the call reports that no stream is on air, with an invalid user id

### Functional

- [sceGameLiveStreamingGetProgramInfo](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) returns success without producing program data because AnyPS5 never starts a live stream. This matches the successful no-op behavior in [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/game_live_streaming/gamelivestreaming.cpp)
