# libSceVoiceQoS

## change [VoiceQoS endpoint ID guest outputs]

- [VoiceQoS endpoint ID guest outputs](../../../core/libs/prx/libSceVoiceQoS/Export.cpp) - the console result for an unwritable or unreadable non-null pointer in `sceVoiceQoSGetLocalEndpoint`, `sceVoiceQoSGetRemoteEndpoint`, `sceVoiceQoSCreateLocalEndpoint`, `sceVoiceQoSCreateRemoteEndpoint`, `sceVoiceQoSConnect`, `sceVoiceQoSReadPacket` or `sceVoiceQoSWritePacket` is unmeasured. AnyPS5 validates destinations upfront under `GuestArena::HostWrite` with `GuestWritable_nid_no_patch` and copies via `GuestCopyIn_nid_no_patch`/`GuestCopyOut_nid_no_patch`, returning `SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID` and restoring `errno` while leaving endpoint and connection state untouched. Closes when: measured on hardware. Labels: console, tested.
