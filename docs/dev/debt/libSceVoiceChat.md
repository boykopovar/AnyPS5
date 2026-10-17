# libSceVoiceChat technical debt

### Unknown function info

- [libSceVoiceChat](../../../core/libs/prx/libSceVoiceChat/Export.cpp) (libSceVoiceChat) - voice chat channel management, audio event, and gain control interfaces; stubs throw until voice chat daemon subsystem is implemented
- [sceVoiceChatRequestCreateVoiceChatGroup, sceVoiceChatRequestJoinVoiceChatGroup, sceVoiceChatRequestLeaveVoiceChatGroup, sceVoiceChatRequestDeleteVoiceChatGroup](../../../core/libs/prx/libSceVoiceChat/Export.cpp) (libSceVoiceChat) - assumed to take a request id and two pointers like the other `sceVoiceChatRequest*` calls; offline, every argument is ignored and `SIGNED_OUT` is returned, as for those
