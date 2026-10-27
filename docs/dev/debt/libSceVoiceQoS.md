# libSceVoiceQoS technical debt

### Silent stubs

- [sceVoiceQoSReadPacket](../../../core/libs/prx/libSceVoiceQoS/Export.cpp) (libSceVoiceQoS) - network voice packet streaming is not implemented: it returns 0 and writes size 0 (indicating no packet is available). Aliens: Fireteam Elite (PPSA03393) calls it on its startup path (`0x76D14C`-`0x76D16E`) and stops with `not implemented` if it throws
- [sceVoiceQoSWritePacket](../../../core/libs/prx/libSceVoiceQoS/Export.cpp) (libSceVoiceQoS) - network voice packet streaming is not implemented: it returns 0 leaving `*size` untouched (treating all input bytes as accepted). Aliens: Fireteam Elite (PPSA03393) sets `*size` to the packet size before `WritePacket` (`0x76CEB9`-`0x76CEC4`), compares `*size` against that input size upon return (`0x76CF07`-`0x76CF10`) to verify all bytes were accepted, and stops with `not implemented` if it throws

### Unknown function info

- [sceVoiceQoSSetMode and sceVoiceQoSTerminate](../../../core/libs/prx/libSceVoiceQoS/Export.cpp): these exports are absent from the supplied module. Their argument declarations still require another firmware version or callers.

### Functional

- [libSceVoiceQoS](../../../core/libs/prx/libSceVoiceQoS/Export.cpp): initialization arguments, initialization errors, endpoint and connection argument lists, and the no-argument End path are confirmed by the supplied module. The host still tracks endpoints without initializing, starting or stopping libSceVoice or creating its ports. Device-dependent port creation, deletion and connection errors remain unimplemented; connecting an uncreated port throws. End clears host state but does not execute the confirmed voice stop/end sequence or propagate its result.
- [sceVoiceQoSCreateLocalEndpoint, sceVoiceQoSCreateRemoteEndpoint and sceVoiceQoSConnect](../../../core/libs/prx/libSceVoiceQoS/Export.cpp): local ID zero and its single-slot limit, remote/connection slot allocation from zero, seven/fifteen slot capacities, exhaustion outputs, and reuse of released slots are recovered. Remote endpoint deletion clears the first matching connection. Shared encoder ownership, codec parameters and packet accounting are still missing.
- [sceVoiceQoSSetLocalEndpointAttribute and sceVoiceQoSSetRemoteEndpointAttribute](../../../core/libs/prx/libSceVoiceQoS/Export.cpp): local selectors 0/2 accept four-byte values and 1/3 one-byte values; remote selector 7 accepts one byte. Unsupported selectors and sizes return the confirmed invalid-argument error. Valid requests throw until voice-port volume/mute forwarding is implemented; they no longer silently store arbitrary bytes.
- [sceVoiceQoSGetLocalEndpoint and sceVoiceQoSGetRemoteEndpoint](../../../core/libs/prx/libSceVoiceQoS/Export.cpp): all in-range connection slots are queryable, including unused slots returning endpoint ID -1. Disconnecting an unused in-range slot succeeds. These state rules are confirmed; the packet, status and attribute getter bodies containing `NotImplemented_nid_no_patch` remain unchanged.
- [sceVoiceQoSEnableChat, sceVoiceQoSDisableChat and sceVoiceQoSInitHQ](../../../core/libs/prx/libSceVoiceQoS/Export.cpp): zero-argument chat wrappers validate initialization and throw because the chat backend is unimplemented; `sceVoiceQoSInitHQ` forwards directly to `sceVoiceQoSInit`.
