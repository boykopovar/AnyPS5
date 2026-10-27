# libSceSystemLogger2NativeQueueClient technical debt

### Unknown function info

- [sceSystemLogger2NativeQueueClientCheckAndMaskData, sceSystemLogger2NativeQueueClientGetEvent, sceSystemLogger2NativeQueueClientSetDeliveryEnable, sceSystemLogger2NativeQueueClientSetDeliveryResult and sceSystemLogger2NativeQueueClientSetNativeQueueConfig](../../../core/libs/prx/libSceSystemLogger2NativeQueueClient/Export.cpp) (libSceSystemLogger2NativeQueueClient) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSystemLogger2NativeQueueClient.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
