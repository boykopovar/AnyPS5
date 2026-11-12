#include "prx/libSceSystemLogger2NativeQueueClient/libSceSystemLogger2NativeQueueClient.hpp"

#include <cstddef>
#include <cstdlib>
#include <vector>

namespace {

constexpr int invalidArgument = static_cast<int>(0x818C0002u);
constexpr int notInitialized = static_cast<int>(0x818C0004u);
constexpr int getEventResult = static_cast<int>(0x818C0006u);
constexpr std::size_t eventBytes = 0x72F4;
constexpr std::size_t checkResultBytes = 0x40;
constexpr std::size_t guard = 0x20;

void Require(bool value) {
    if (!value) std::abort();
}

void RequireZeroedThenUntouched(const std::vector<unsigned char>& block, std::size_t zeroed) {
    for (std::size_t i = 0; i < block.size(); ++i) Require(block[i] == (i < zeroed ? 0 : 0x5A));
}

} // namespace

int main() {
    for (int call = 0; call < 2; ++call) {
        Require(sceSystemLogger2NativeQueueClientCheckAndMaskData() == notInitialized);
        Require(sceSystemLogger2NativeQueueClientSetDeliveryEnable() == notInitialized);
        Require(sceSystemLogger2NativeQueueClientSetNativeQueueConfig() == notInitialized);
        Require(sceSystemLogger2NativeQueueClientSetEventFilter() == notInitialized);
        Require(sceSystemLogger2NativeQueueClientGetEvent2() == notInitialized);
        Require(sceSystemLogger2NativeQueueClientTerminate() == notInitialized);
        Require(libSceSystemLogger2NativeQueueClientUnknown_AuwwRl6zAcU_nid_no_patch() == notInitialized);

        Require(sceSystemLogger2NativeQueueClientGetEvent(nullptr) == invalidArgument);
        std::vector<unsigned char> event(eventBytes + guard, 0x5A);
        Require(sceSystemLogger2NativeQueueClientGetEvent(event.data()) == getEventResult);
        RequireZeroedThenUntouched(event, eventBytes);

        std::vector<unsigned char> data(0x40, 0x5A);
        Require(sceSystemLogger2NativeQueueClientCheckData(data.data(), nullptr) == invalidArgument);
        Require(sceSystemLogger2NativeQueueClientCheckData(nullptr, nullptr) == invalidArgument);
        std::vector<unsigned char> result(checkResultBytes + guard, 0x5A);
        Require(sceSystemLogger2NativeQueueClientCheckData(data.data(), result.data()) == notInitialized);
        RequireZeroedThenUntouched(result, checkResultBytes);
        std::vector<unsigned char> second(checkResultBytes + guard, 0x5A);
        Require(sceSystemLogger2NativeQueueClientCheckData(nullptr, second.data()) == notInitialized);
        RequireZeroedThenUntouched(second, checkResultBytes);
        for (const unsigned char byte : data) Require(byte == 0x5A);
    }
    return 0;
}
