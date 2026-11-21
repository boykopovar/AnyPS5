# libSceVoice

## change [sceVoiceSetThreadsParams, sceVoiceStart, sceVoiceCreatePort, sceVoiceReadFromOPort, sceVoiceWriteToIPort, sceVoiceDeviceInCustomerData and sceVoiceUpdatePort]

- [sceVoiceInit, sceVoiceSetThreadsParams, sceVoiceStart, sceVoiceCreatePort, sceVoiceReadFromOPort, sceVoiceWriteToIPort, sceVoiceDeviceInCustomerData and sceVoiceUpdatePort](../../../core/libs/prx/libSceVoice/Export.cpp) (libSceVoice) - returning InvalidArgument for an unreadable parameter struct or gain pointer, or an unwritable port id or size pointer, is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
