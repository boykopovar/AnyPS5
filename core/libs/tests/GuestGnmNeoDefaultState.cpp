#include "prx/libSceGnmDriverForNeoMode/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

namespace {

using StateWriter = int (APS5_VABI *)(std::uint32_t*, std::uint32_t);

constexpr std::uint32_t Untouched = 0xa5a5a5a5u;
constexpr std::uint32_t BufferWords = 0x110;

std::uint32_t g_words[BufferWords];

void Require(bool value) { if (!value) std::abort(); }

std::uint32_t* Fresh() {
    for (auto& word : g_words) word = Untouched;
    return g_words;
}

void RequireAt(std::uint32_t at, std::initializer_list<std::uint32_t> expected) {
    for (const auto word : expected) Require(g_words[at++] == word);
}

void RequireUntouchedFrom(std::uint32_t at) {
    for (; at < BufferWords; ++at) Require(g_words[at] == Untouched);
}

void CheckHardwareState(StateWriter writer, std::uint32_t stateWords, std::initializer_list<std::uint32_t> tail) {
    Require(writer(Fresh(), 0xff) == 0);
    RequireUntouchedFrom(0);

    Require(writer(Fresh(), 0x100) == 0x100);
    RequireAt(0, {0xc0012800u, 0x80000000u, 0x80000000u, 0xc0001200u, 0, 0xc0055800u, 0x2ec47fc0u, 0xffffffffu, 0, 0, 0, 10});
    RequireAt(12, {0xc0017600u, 0x216, 0xffffffffu, 0xc0017600u, 0x217, 0xffffffffu});
    const std::uint32_t end = 12 + stateWords;
    RequireAt(end - static_cast<std::uint32_t>(tail.size()), tail);
    const std::uint32_t padding = 0x100 - end - 1;
    RequireAt(end, {0xc0001000u | ((padding - 1) << 16), 0});
    for (std::uint32_t at = end + 2; at < 0x100; ++at) Require(g_words[at] == Untouched);
    RequireUntouchedFrom(0x100);

    Require(writer(Fresh(), 0x10f) == 0x100);
    RequireUntouchedFrom(0x100);
}

void CheckContextState(StateWriter writer, std::uint32_t packetWords, std::uint32_t stateWords, std::initializer_list<std::uint32_t> tail) {
    Require(writer(Fresh(), packetWords - 1) == 0);
    RequireUntouchedFrom(0);
    Require(writer(Fresh(), packetWords + 1) == 0);
    RequireUntouchedFrom(0);

    Require(writer(Fresh(), packetWords) == static_cast<int>(packetWords));
    RequireAt(0, {0xc0012800u, 0x80000000u, 0x80000000u, 0xc0001200u, 0});
    RequireAt(stateWords - static_cast<std::uint32_t>(tail.size()), tail);
    RequireUntouchedFrom(stateWords);
}

} // namespace

int main() {
    CheckHardwareState(sceGnmDrawInitDefaultHardwareState, 115, {0x100, 4, 0xc0017900u, 0x200, 0xe0000000u});
    CheckHardwareState(sceGnmDrawInitDefaultHardwareState200, 131, {0x200, 0xe0000000u, 0xc0017900u, 0x40000258u, 0x6d007fu});
    CheckHardwareState(sceGnmDrawInitDefaultHardwareState350, 134, {0x200, 0xe0000000u, 0xc0017900u, 0x40000258u, 0x6d007fu});
    CheckContextState(sceGnmDrawInitToDefaultContextState, 0x20, 19, {0xff00ff00u, 0xff00, 0xc00d1000u, 0});
    CheckContextState(sceGnmDrawInitToDefaultContextState400, 0x100, 102, {0x40000258u, 0x6d007fu, 0xc09a1000u, 0});
}
