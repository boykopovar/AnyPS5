#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceVoiceQoSInit(void*, std::uint32_t, std::int32_t);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int argumentInvalid = static_cast<int>(0x804E0902);
constexpr int initialized = static_cast<int>(0x804E0905);
constexpr std::uint32_t memorySize = 0x40000;
constexpr std::uint8_t filler = 0xAA;

std::int32_t AppType() {
    return static_cast<std::int32_t>(std::strtoul(Testing::RequireArgument(0, "app type").c_str(), nullptr, 16));
}

std::vector<std::uint8_t> FilledMemory() {
    return std::vector<std::uint8_t>(memorySize, filler);
}

void RequireUntouched(const std::vector<std::uint8_t>& memory) {
    for (std::size_t index = 0; index < memory.size(); ++index) {
        RequireEqual(memory[index], filler, "memory byte " + std::to_string(index) + " after rejected init");
    }
}

const Case nullMemory{"Init_NullMemory_ReturnsArgumentInvalid", [] {
    const auto appType = AppType();
    RequireEqual(sceVoiceQoSInit(nullptr, memorySize, appType), argumentInvalid, "null memory with the full size");
    RequireEqual(sceVoiceQoSInit(nullptr, 0, appType), argumentInvalid, "null memory with zero size");
}};

const Case invalidSize{"Init_MemorySizeOtherThan0x40000_ReturnsArgumentInvalidWithoutWriting", [] {
    const auto appType = AppType();
    auto memory = FilledMemory();
    for (const std::uint32_t size : {0u, 1u, 0x100u, 0x3FFFFu, 0x40001u, 0x80000u, 0xFFFFFFFFu}) {
        RequireEqual(sceVoiceQoSInit(memory.data(), size, appType), argumentInvalid, "memory size " + std::to_string(size));
    }
    RequireUntouched(memory);
}};

const Case invalidAppType{"Init_UnsupportedAppType_ReturnsArgumentInvalidWithoutWriting", [] {
    auto memory = FilledMemory();
    for (const std::int32_t type : {0, 1, 0x08000000, 0x20000001, 0x30000000, 0x40000000, static_cast<std::int32_t>(0x80000000), -1}) {
        RequireEqual(sceVoiceQoSInit(memory.data(), memorySize, type), argumentInvalid, "app type " + std::to_string(type));
    }
    RequireUntouched(memory);
}};

const Case validInit{"Init_ValidArguments_SucceedsOnceThenReportsInitialized", [] {
    const auto appType = AppType();
    auto memory = FilledMemory();
    RequireEqual(sceVoiceQoSInit(memory.data(), memorySize, appType), 0, "first init");
    RequireEqual(sceVoiceQoSInit(memory.data(), memorySize, appType), initialized, "second init");
    RequireEqual(sceVoiceQoSInit(nullptr, 0, 0), initialized, "invalid arguments after init");
}};

} // namespace
