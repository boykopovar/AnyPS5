# libSceAutoMounterClient

## change [sceAutoMounterClientRegisterCallback, sceAutoMounterClientTerm and sceAutoMounterClientUnregisterCallback]

- [sceAutoMounterClientRegisterCallback](../../../core/libs/prx/libSceAutoMounterClient/Export.cpp) (libSceAutoMounterClient) - its name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAutoMounterClient.c), but no credible prototype was found. It is declared without parameters solely to resolve known imports; this is not an ABI claim and callers must not call it until the signature is verified. `sceAutoMounterClientTerm` and `sceAutoMounterClientUnregisterCallback` return 0 as no-ops: the client holds no host state and nothing is registered, so there is nothing to tear down.

## change [sceAutoMounterClientInit]

- [sceAutoMounterClientInit](../../../core/libs/prx/libSceAutoMounterClient/Export.cpp) (libSceAutoMounterClient) - returns 0 and ignores the `PthreadAttr*`. No host thread is started, so no attributes are applied. Interactions with a real automounter service are not implemented.

## change [sceAutoMounterClientGetUsbDeviceInfo and sceAutoMounterClientGetUsbDeviceList]

- [sceAutoMounterClientGetUsbDeviceInfo and sceAutoMounterClientGetUsbDeviceList](../../../core/libs/prx/libSceAutoMounterClient/Export.cpp) (libSceAutoMounterClient) - the host reports no USB storage devices. `sceAutoMounterClientGetUsbDeviceList` writes 0 into the caller's count and returns 0. `sceAutoMounterClientGetUsbDeviceInfo` validates the device name through the same bounded guest-string copy and `/dev%s` match the 13.42 module disassembly (sha256 6aaa6bd4b3e0c2f62fcfd4b8ed6fb738be7853c2b59a3ee89c032f36157970f0) uses, and returns `0x80F40002` because no matching device exists. Neither export is measured on a console, and the structure written by the console into `AutoMounterUsbDeviceInfo` remains unverified.
