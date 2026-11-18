# libSceSystemLogger2Delivery

## change [sceSystemLogger2DeliveryGetBatchResult, sceSystemLogger2DeliveryGetResult, sceSystemLogger2DeliverySetBatchDeliveryEvent and sceSystemLogger2DeliveryTerminate]

- [sceSystemLogger2DeliverySetBatchDeliveryEvent](../../../core/libs/prx/libSceSystemLogger2Delivery/Export.cpp) (libSceSystemLogger2Delivery) - name and NID are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSystemLogger2Delivery.c), but no credible prototype was found. The declaration has no parameters solely to resolve known imports; this is not an ABI claim and callers must not call it until the signature is verified. `sceSystemLogger2DeliveryGetBatchResult`, `sceSystemLogger2DeliveryGetResult` and `sceSystemLogger2DeliveryTerminate` return 0: no host delivery pipeline is attached, so there is no result to report and no state to tear down.

## change [sceSystemLogger2DeliveryResume, sceSystemLogger2DeliverySuspend, sceSystemLogger2DeliveryInitialize]

- [sceSystemLogger2DeliveryResume, sceSystemLogger2DeliverySuspend and sceSystemLogger2DeliveryInitialize](../../../core/libs/prx/libSceSystemLogger2Delivery/Export.cpp) return 0. No host delivery pipeline is attached, so there is nothing to initialize, suspend or resume. Only the names and NIDs are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscesystemlogger2delivery-84263dcdd4.jsonl).
