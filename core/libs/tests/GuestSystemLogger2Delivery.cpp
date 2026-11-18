#include "prx/libSceSystemLogger2Delivery/libSceSystemLogger2Delivery.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    constexpr int notInitialized = static_cast<int>(0x818C0004u);
    constexpr int notAvailable = static_cast<int>(0x818C000Eu);

    Require(sceSystemLogger2DeliveryGetResult() == notInitialized);
    Require(sceSystemLogger2DeliveryGetBatchResult() == notInitialized);
    Require(sceSystemLogger2DeliverySuspend() == notInitialized);
    Require(sceSystemLogger2DeliveryResume() == notInitialized);
    Require(sceSystemLogger2DeliverySetBatchDeliveryEvent() == notInitialized);

    Require(sceSystemLogger2DeliveryInitialize() == 0);

    Require(sceSystemLogger2DeliveryGetResult() == notAvailable);
    Require(sceSystemLogger2DeliveryGetBatchResult() == notAvailable);
    Require(sceSystemLogger2DeliverySuspend() == 0);
    Require(sceSystemLogger2DeliveryResume() == 0);
    Require(sceSystemLogger2DeliverySetBatchDeliveryEvent() == notAvailable);

    Require(sceSystemLogger2DeliveryTerminate() == 0);
    return 0;
}
