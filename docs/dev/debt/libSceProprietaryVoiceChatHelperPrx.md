# libSceProprietaryVoiceChatHelperPrx technical debt

### Silent stubs

- [libSceProprietaryVoiceChatHelperPrx](../../../core/libs/prx/libSceProprietaryVoiceChatHelperPrx/Export.cpp) implements only the no-system-voice-chat path: the host has no system voice chat for a title's own voice chat to announce itself to, so `sceProprietaryVoiceChatHelperInitialize` and `sceProprietaryVoiceChatHelperTerminate` only track whether the library is initialized, `sceProprietaryVoiceChatHelperSetVoiceChatState` returns success and does nothing, and `sceProprietaryVoiceChatHelperGetVoiceChatUsageState` returns inactive state `0`. There is nothing to notify or consume. Initializing twice, and any call before `sceProprietaryVoiceChatHelperInitialize` or after `sceProprietaryVoiceChatHelperTerminate`, throws, as the console's error codes for this library are unknown. Whether the console checks initialization order at all is unverified
