# libSceSystemLogger2

## add Console service

- Timestamp and event status outputs: sceSystemLogger2GetTimestamp, sceSystemLogger2GetEventStatus, and sceSystemLogger2SetEventAsyncInternal validate and copy guest event names and output structs with CopyGuestString_nid_no_patch and GuestCopyOut_nid_no_patch while still returning SCE_SYSTEM_LOGGER2_ERROR_NOT_AVAILABLE when the system logger service is absent; verified on host in core/libs/prx/libSceSystemLogger2/tests/LifecycleAndEvents.cpp.
