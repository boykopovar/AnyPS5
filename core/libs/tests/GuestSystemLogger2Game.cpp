#include "prx/libSceSystemLogger2Game/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int invalidArgument = static_cast<int>(0x818c0002u);

void Require(bool value) { if (!value) std::abort(); }

void CheckTimestamp() {
    SceSystemLogger2Timestamp timestamp{};
    std::memset(&timestamp, 0xaa, sizeof(timestamp));
    Require(sceSystemLogger2GameGetTimestamp(&timestamp) == 0);
    Require(timestamp.tick >= 62135596800000000ull);
    Require(timestamp.tick % 1000000 == 0);
    Require(std::strcmp(timestamp.clockSource, "secureClock") == 0);
    for (std::size_t index = sizeof("secureClock"); index < sizeof(timestamp.clockSource); ++index) {
        Require(timestamp.clockSource[index] == 0);
    }
}

}

int main() {
    static_assert(sizeof(SceSystemLogger2Timestamp) == 0x30);
    Require(sceSystemLogger2GameGetTimestamp(nullptr) == invalidArgument);
    CheckTimestamp();
    errno = 71;
    Require(sceSystemLogger2GameGetTimestamp(reinterpret_cast<SceSystemLogger2Timestamp*>(0x10)) == invalidArgument);
    Require(errno == 71);
    CheckTimestamp();
}
