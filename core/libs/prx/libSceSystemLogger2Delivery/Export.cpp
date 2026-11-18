#include "prx/libSceSystemLogger2Delivery/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceSystemLogger2DeliveryGetBatchResult() {
    return 0;
}

int APS5_VABI sceSystemLogger2DeliveryGetResult() {
    return 0;
}

int APS5_VABI sceSystemLogger2DeliverySetBatchDeliveryEvent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemLogger2DeliveryTerminate() {
    return 0;
}

int APS5_VABI sceSystemLogger2DeliveryResume() {
    return 0;
}

int APS5_VABI sceSystemLogger2DeliverySuspend() {
    return 0;
}

int APS5_VABI sceSystemLogger2DeliveryInitialize() {
    return 0;
}

}
