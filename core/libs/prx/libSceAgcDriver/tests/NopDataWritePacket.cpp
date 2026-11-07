#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

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

    for (std::uint32_t len = 1; len <= 4; ++len) {
        storage.fill(std::byte{0xaa});
        const auto lenWords = sceAgcDriverNopDataWritePacket(storage.data() + 4, 0xaabbccddU, data.data(), len);
        std::memcpy(&header, storage.data() + 4, sizeof(header));
        std::memcpy(&address, storage.data() + 8, sizeof(address));
        if (lenWords != 3 || header != 0xc0011000U || address != 0xaabbccddU) return 9 + static_cast<int>(len);
        if (!std::equal(data.begin(), data.begin() + len, storage.begin() + 12)) return 14 + static_cast<int>(len);
        for (std::size_t pad = 12 + len; pad < 16; ++pad) {
            if (storage[pad] != std::byte{0}) return 19 + static_cast<int>(len);
        }
        if (storage[3] != std::byte{0xaa} || storage[16] != std::byte{0xaa}) return 24 + static_cast<int>(len);
    }

    const std::vector<std::byte> largest(65532, std::byte{0x5a});
    std::vector<std::byte> largestStorage(65540 + 8, std::byte{0xaa});
    const auto largestWords = sceAgcDriverNopDataWritePacket(largestStorage.data() + 4, 0x11223344, largest.data(), 65532);
    std::memcpy(&header, largestStorage.data() + 4, sizeof(header));
    std::memcpy(&address, largestStorage.data() + 8, sizeof(address));
    if (largestWords != 16385 || header != 0xffff1000U || address != 0x11223344U) return 30;
    if (largestStorage[12] != std::byte{0x5a} || largestStorage[12 + 65531] != std::byte{0x5a}) return 31;
    if (largestStorage[4 + 65540] != std::byte{0xaa} || largestStorage[3] != std::byte{0xaa}) return 32;

    const std::vector<std::byte> wrapped(65536, std::byte{0x5a});
    std::vector<std::byte> wrappedStorage(65544 + 8, std::byte{0xaa});
    const auto wrappedWords = sceAgcDriverNopDataWritePacket(wrappedStorage.data() + 4, 0x55667788, wrapped.data(), 65536);
    std::memcpy(&header, wrappedStorage.data() + 4, sizeof(header));
    std::memcpy(&address, wrappedStorage.data() + 8, sizeof(address));
    if (wrappedWords != 16386 || header != 0xc0001000U || address != 0x55667788U) return 33;
    if (wrappedStorage[12] != std::byte{0x5a} || wrappedStorage[12 + 65535] != std::byte{0x5a}) return 34;
    if (wrappedStorage[4 + 65544] != std::byte{0xaa} || wrappedStorage[3] != std::byte{0xaa}) return 35;
    return 0;
}
