# libSceVoiceQoS

## change [sceVoiceQoSSetLocalEndpointAttribute]

- [sceVoiceQoSSetLocalEndpointAttribute](../../../core/libs/prx/libSceVoiceQoS/Export.cpp): local selectors 0/2 forward a four-byte float to sceVoiceSetVolume on the input/output port and 1/3 a one-byte flag to sceVoiceSetMuteFlag; unsupported selectors and sizes return the confirmed invalid-argument error. Without a created local endpoint the call returns the invalid-argument error instead of addressing libSceVoice port 0, which is a real handle a title can own. Value scalars are copied through the checked guest input path before being forwarded to libSceVoice; for an unreadable non-null value, AnyPS5 returns the existing invalid-argument error without changing a voice port, as a host-safety fallback. The console's result for unreadable non-null value pointers is unmeasured. Remote selector 7 still throws until remote endpoint ports exist.
