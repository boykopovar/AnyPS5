# libSceVoiceQoS

## add Functional

- [sceVoiceQoSGetStatus and sceVoiceQoSDebugGetStatus](../../../core/libs/prx/libSceVoiceQoS/Export.cpp) (libSceVoiceQoS) - throw after the first `sceVoiceQoSReadPacket` or `sceVoiceQoSWritePacket` call made while the library is initialized, in the whole process: firmware 12.70's `libSceVoiceQoS.sprx` (SHA-256 `31095245c65631faed9124d58578e7bdc523a5d6e160b713bae62542725a1002`, exports at `0x1ce0` and `0x1e80`, read from disassembly, not run on a console) updates the status fields from packet contents AnyPS5 does not parse, also on an unconnected slot whose call AnyPS5 rejects, and never resets the field debug status 205 reads. GetStatus also throws for statuses 0, 1 and 4 of an unused connection, which the module reads through remote endpoint index -1, outside its table; status 4's timeout also writes there. Before packet traffic both follow the module. Both take a connection ID below the 7 or 15 limit and a four-byte value; other IDs, sizes and a null value return `0x804E0902`. GetStatus takes status IDs 0 to 4: 0 and 1 are the remote endpoint's bitrate fields (22533 for app type `0x10000000`, 7300 for `0x20000000`, set when the remote endpoint is created), 2 the connection's copy of it, 3 a float that stays 0.0 until a packet sets its flag, and 4 is 1 until more than 120 seconds of `clock_gettime(CLOCK_MONOTONIC)`, compared as floats, have passed since the connection was made, then 0. DebugGetStatus takes 200 to 210, each a field of the connection record: 210 is 1.0 and the others 0. For an unwritable non-null value both return `0x804E0902` as a host-safety fallback; the console result is unmeasured. Closes when: sceVoiceQoSReadPacket and sceVoiceQoSWritePacket process packets.

## change [sceVoiceQoSGetLocalEndpoint and sceVoiceQoSGetRemoteEndpoint]

- [sceVoiceQoSGetLocalEndpoint and sceVoiceQoSGetRemoteEndpoint](../../../core/libs/prx/libSceVoiceQoS/Export.cpp): all in-range connection slots are queryable, including unused slots returning endpoint ID -1. Disconnecting an unused in-range slot succeeds. These state rules are confirmed.

## change [sceVoiceQoSSetMode and sceVoiceQoSTerminate]

- [sceVoiceQoSSetMode and sceVoiceQoSTerminate](../../../core/libs/prx/libSceVoiceQoS/Export.cpp): these exports are absent from the supplied module and from firmware 12.70's `libSceVoiceQoS.sprx` (SHA-256 `31095245c65631faed9124d58578e7bdc523a5d6e160b713bae62542725a1002`). Their argument declarations still require another firmware version or callers; sceVoiceQoSTerminate forwards to sceVoiceQoSEnd as an inferred alias.
