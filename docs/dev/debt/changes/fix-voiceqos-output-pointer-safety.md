# libSceVoiceQoS

## add Functional

- [sceVoiceQoSCreateLocalEndpoint, sceVoiceQoSCreateRemoteEndpoint, sceVoiceQoSConnect, sceVoiceQoSReadPacket, sceVoiceQoSWritePacket, sceVoiceQoSGetLocalEndpoint, sceVoiceQoSGetRemoteEndpoint](../../../core/libs/prx/libSceVoiceQoS/Export.cpp) - output identifiers and buffer sizes are validated upfront with `GuestWritable_nid_no_patch` before mutating endpoints or copying results via `GuestCopyOut_nid_no_patch`. Packet writes copy size using `GuestCopyIn_nid_no_patch`. All error paths restore caller `errno`.
