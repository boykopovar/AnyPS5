# libSceSystemLogger2NativeQueueClient

## change no credible prototypes were found

- [sceSystemLogger2NativeQueueClientCheckAndMaskData, sceSystemLogger2NativeQueueClientGetEvent, sceSystemLogger2NativeQueueClientSetDeliveryEnable, sceSystemLogger2NativeQueueClientSetDeliveryResult and sceSystemLogger2NativeQueueClientSetNativeQueueConfig](../../../core/libs/prx/libSceSystemLogger2NativeQueueClient/Export.cpp) (libSceSystemLogger2NativeQueueClient) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSystemLogger2NativeQueueClient.c), but no credible prototypes were found. Except for `GetEvent`, which takes the pointer it zeroes before initialization, the declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

## change sceSystemLogger2NativeQueueClientSetBatchDeliveryResult, sceSystemLogger2NativeQueueClientSetEventFilter

- [sceSystemLogger2NativeQueueClientSetBatchDeliveryResult, sceSystemLogger2NativeQueueClientSetEventFilter, sceSystemLogger2NativeQueueClientCheckData, sceSystemLogger2NativeQueueClientGetEvent2, sceSystemLogger2NativeQueueClientInitialize, sceSystemLogger2NativeQueueClientTerminate](../../../core/libs/prx/libSceSystemLogger2NativeQueueClient/Export.cpp) have unknown signatures and behaviour. The `int` declarations, parameterless except for the two pointers of `CheckData`, are import-resolution placeholders, not confirmed ABI signatures. `SetBatchDeliveryResult` and `Initialize` throw through `NotImplemented_nid_no_patch`; the other four give only the answers before initialization. Only the names and NIDs are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscesystemlogger2nativequeueclient-20515566b1.jsonl).

## change Unidentified export

- [Unidentified export `AuwwRl6zAcU`](../../../core/libs/prx/libSceSystemLogger2NativeQueueClient/Export.cpp) is an import-resolution declaration that gives only the answer before initialization, `0x818C0004` (see the uninitialized answers). Its name, parameter list, return type and behavior after initialization are unverified; the parameterless declaration is not an ABI contract.
