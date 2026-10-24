#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

int main() {
    std::array<std::byte, 24> storage;
    storage.fill(std::byte{0xaa});
    const std::array<std::byte, 5> data{std::byte{0x11}, std::byte{0x22}, std::byte{0x33}, std::byte{0x44}, std::byte{0x55}};
    const auto words = sceAgcDriverNopDataWritePacket(storage.data() + 4, 0x12345678, data.data(), static_cast<std::uint32_t>(data.size()));
    std::uint32_t header = 0;
    std::uint32_t address = 0;
    std::memcpy(&header, storage.data() + 4, sizeof(header));
    std::memcpy(&address, storage.data() + 8, sizeof(address));
    if (words != 4 || header != 0xc0021000U || address != 0x12345678U) return 1;
    if (!std::equal(data.begin(), data.end(), storage.begin() + 12)) return 2;
    if (storage[17] != std::byte{0} || storage[18] != std::byte{0} || storage[19] != std::byte{0}) return 3;
    if (storage[3] != std::byte{0xaa} || storage[20] != std::byte{0xaa}) return 4;

    storage.fill(std::byte{0xaa});
    const auto emptyWords = sceAgcDriverNopDataWritePacket(storage.data() + 4, 0x87654321, nullptr, 5);
    std::memcpy(&header, storage.data() + 4, sizeof(header));
    std::memcpy(&address, storage.data() + 8, sizeof(address));
    if (emptyWords != 2 || header != 0xc0021000U || address != 0x87654321U) return 5;
    if (storage[12] != std::byte{0} || storage[19] != std::byte{0} || storage[20] != std::byte{0xaa}) return 6;

    storage.fill(std::byte{0xaa});
    const auto zeroBytesWords = sceAgcDriverNopDataWritePacket(storage.data() + 4, 0x01020304, data.data(), 0);
    std::memcpy(&header, storage.data() + 4, sizeof(header));
    std::memcpy(&address, storage.data() + 8, sizeof(address));
    if (zeroBytesWords != 2 || header != 0xc0001000U || address != 0x01020304U) return 7;
    if (storage[12] != std::byte{0xaa}) return 8;
    return 0;
}
