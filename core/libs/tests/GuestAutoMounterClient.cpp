#include "prx/libSceAutoMounterClient/include/Api.hpp"
#include <cstdlib>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

constexpr int AUTOMOUNTER_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80F40002u);

}

int main() {
    AutoMounterUsbDeviceInfo info{};
    char deviceNames[1][64]{};
    std::int32_t count = 0;

    Require(sceAutoMounterClientGetUsbDeviceInfo("usb0", &info) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);
    Require(sceAutoMounterClientGetUsbDeviceInfo(reinterpret_cast<const char*>(0x10), &info) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);
    Require(sceAutoMounterClientGetUsbDeviceList(nullptr, &count) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);
    Require(sceAutoMounterClientGetUsbDeviceList(deviceNames, nullptr) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);
    Require(sceAutoMounterClientGetUsbDeviceList(nullptr, nullptr) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);

    return 0;
}
