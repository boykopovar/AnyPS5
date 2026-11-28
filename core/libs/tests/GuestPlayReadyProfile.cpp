#include "prx/libScePlayReady4/libScePlayReady4.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libScePlayReady4 profile check failed at line %u\n", location.line());
    std::abort();
}

std::uint64_t ReadU64(const std::byte* address) {
    std::uint64_t value = 0;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

constexpr std::size_t EntryBytes = 24;

} // namespace

int main() {
    std::array<std::byte, EntryBytes * 4> buffer;
    buffer.fill(std::byte{0x5A});

    scePlayReadyProfileStart(buffer.data(), 4);
    scePlayReadyProfileEnter(7, 8, 9);
    Require(buffer[0] == std::byte{1});
    Require(buffer[1] == std::byte{7});
    Require(buffer[2] == std::byte{8});
    Require(buffer[3] == std::byte{9});
    Require(ReadU64(buffer.data() + 8) != 0);
    scePlayReadyProfileLeave();
    Require(ReadU64(buffer.data() + 16) < 10'000'000ull);

    scePlayReadyProfileEnter(1, 2, 3);
    Require(buffer[EntryBytes + 0] == std::byte{1});
    Require(buffer[EntryBytes + 1] == std::byte{1});
    scePlayReadyProfileEnter(4, 5, 6);
    Require(buffer[2 * EntryBytes + 0] == std::byte{2});
    Require(buffer[2 * EntryBytes + 1] == std::byte{4});
    scePlayReadyProfileLeave();
    Require(ReadU64(buffer.data() + 2 * EntryBytes + 16) < 10'000'000ull);
    Require(buffer[EntryBytes + 16] == std::byte{0x5A});
    scePlayReadyProfileLeave();
    Require(ReadU64(buffer.data() + EntryBytes + 16) < 10'000'000ull);

    const auto balanced = buffer;
    scePlayReadyProfilePause();
    scePlayReadyProfileEnter(9, 9, 9);
    Require(buffer == balanced);
    scePlayReadyProfileResume();
    scePlayReadyProfileEnter(3, 4, 5);
    Require(buffer[3 * EntryBytes + 0] == std::byte{1});
    Require(buffer[3 * EntryBytes + 1] == std::byte{3});
    scePlayReadyProfileLeave();

    const auto full = buffer;
    scePlayReadyProfileEnter(1, 1, 1);
    Require(buffer == full);

    scePlayReadyProfileStop();
    const auto stopped = buffer;
    scePlayReadyProfileEnter(1, 1, 1);
    Require(buffer == stopped);

    scePlayReadyProfileStart(buffer.data(), 4);
    scePlayReadyProfileEnter(6, 7, 8);
    Require(buffer[0] == std::byte{1});
    Require(buffer[1] == std::byte{6});
    scePlayReadyProfileLeave();
    scePlayReadyProfileClear();
    Require(buffer[0] == std::byte{0});
    Require(buffer[1] == std::byte{0});
    Require(ReadU64(buffer.data() + 8) == 0);
    Require(ReadU64(buffer.data() + 16) == 0);
    Require(buffer[EntryBytes] != std::byte{0});

    scePlayReadyProfileStop();
    const auto nullStart = buffer;
    scePlayReadyProfileStart(nullptr, 4);
    scePlayReadyProfileEnter(1, 1, 1);
    Require(buffer == nullStart);
    return 0;
}
