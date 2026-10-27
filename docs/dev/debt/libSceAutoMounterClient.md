# libSceAutoMounterClient technical debt

### Unknown function info

- [sceAutoMounterClientRegisterCallback, sceAutoMounterClientTerm and sceAutoMounterClientUnregisterCallback](../../../core/libs/prx/libSceAutoMounterClient/Export.cpp) (libSceAutoMounterClient) - their names and NIDs are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAutoMounterClient.c), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified.
- [sceAutoMounterClientGetUsbDeviceInfo](../../../core/libs/prx/libSceAutoMounterClient/Export.cpp): the 22 metadata bytes at offset 128 remain opaque. The 944-byte output layout, 128-byte device name and three 256-byte arrays with 64-bit lengths are recovered.
- [sceAutoMounterClientInit, sceAutoMounterClientGetUsbDeviceList](../../../core/libs/prx/libSceAutoMounterClient/Export.cpp) (libSceAutoMounterClient) - throw through `NotImplemented_nid_no_patch`. Client initialization thread attributes and device list query interactions with the automounter service remain unimplemented.
