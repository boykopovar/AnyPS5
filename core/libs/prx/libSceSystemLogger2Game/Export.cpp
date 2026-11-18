#include "prx/libSceSystemLogger2Game/include/InventoryExports.hpp"
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int INVALID_ARGUMENT = static_cast<int>(0x818C0002u);
constexpr int NOT_AVAILABLE = static_cast<int>(0x818C000Eu);

constexpr std::int64_t UNIX_EPOCH_DAYS = 719162;
constexpr std::int64_t TICKS_PER_DAY = 24LL * 60 * 60 * 1000000;
constexpr std::uint64_t UNIX_EPOCH_TICK = static_cast<std::uint64_t>(UNIX_EPOCH_DAYS * TICKS_PER_DAY);

struct Timestamp {
    std::uint64_t tick;
    char clockSource[16];
};

std::uint64_t currentNetworkTick() {
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    return UNIX_EPOCH_TICK + static_cast<std::uint64_t>(now);
}

}

extern "C" {

int APS5_VABI sceSystemLogger2GameGetTimestamp(void* timestamp) {
    if (timestamp == nullptr) return INVALID_ARGUMENT;
    Timestamp value{};
    value.tick = currentNetworkTick();
    std::memcpy(value.clockSource, "networkClock", sizeof("networkClock"));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(timestamp, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemLogger2GameSetEvent(const void* event) {
    if (event == nullptr) return INVALID_ARGUMENT;
    return NOT_AVAILABLE;
}

APS5_DUMMY_FUN

}
