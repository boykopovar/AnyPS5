#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" void APS5_VABI sceAudio3dGetDefaultOpenParameters(Audio3dOpenParameters* parameters);

namespace {

using Testing::Case;
using Testing::RequireEqual;
using Testing::RequireThrows;

static_assert(sizeof(Audio3dOpenParameters) == 0x28);
static_assert(offsetof(Audio3dOpenParameters, num_beds) == 0x20);

constexpr unsigned char guardByte = 0xa5;

void RequireDefaults(const void* memory, const std::string& context) {
    Audio3dOpenParameters parameters{};
    std::memcpy(&parameters, memory, 0x20);
    RequireEqual(parameters.size_this, decltype(parameters.size_this){0x20}, context + ": default parameter size");
    RequireEqual(parameters.granularity, decltype(parameters.granularity){256}, context + ": default granularity");
    RequireEqual(parameters.rate, decltype(parameters.rate){0}, context + ": default rate");
    RequireEqual(parameters.max_objects, decltype(parameters.max_objects){512}, context + ": default object limit");
    RequireEqual(parameters.queue_depth, decltype(parameters.queue_depth){2}, context + ": default queue depth");
    RequireEqual(parameters.buffer_mode, decltype(parameters.buffer_mode){2}, context + ": default buffer mode");
    RequireEqual(parameters.pad, decltype(parameters.pad){0}, context + ": reserved parameter bytes");
}

struct GuardedParameters {
    std::array<unsigned char, 8> before;
    alignas(Audio3dOpenParameters) std::array<unsigned char, 0x20> parameters;
    std::array<unsigned char, 8> after;
};

static_assert(offsetof(GuardedParameters, after) == offsetof(GuardedParameters, parameters) + 0x20);

const Case nullDestination{"GetDefaultOpenParameters_NullDestination_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { sceAudio3dGetDefaultOpenParameters(nullptr); }, "null destination");
}};

const Case shortBuffer{"GetDefaultOpenParameters_AnyIncomingSize_WritesExactly32Bytes", [] {
    GuardedParameters guarded;
    std::memset(&guarded, guardByte, sizeof(guarded));
    for (const std::uint64_t size : std::array<std::uint64_t, 6>{0, 0x10, 0x18, 0x20, 0x28, UINT64_MAX}) {
        const std::string context = "incoming size " + std::to_string(size);
        guarded.parameters.fill(guardByte);
        std::memcpy(guarded.parameters.data(), &size, sizeof(size));
        sceAudio3dGetDefaultOpenParameters(reinterpret_cast<Audio3dOpenParameters*>(guarded.parameters.data()));
        RequireDefaults(guarded.parameters.data(), context);
        for (const unsigned char value : guarded.before) RequireEqual(value, guardByte, context + ": byte before the parameters");
        for (const unsigned char value : guarded.after) RequireEqual(value, guardByte, context + ": byte after the parameters");
    }
}};

const Case extendedBuffer{"GetDefaultOpenParameters_FullStructure_LeavesExtendedBytesUnchanged", [] {
    Audio3dOpenParameters parameters;
    std::memset(&parameters, guardByte, sizeof(parameters));
    for (const unsigned char pattern : {static_cast<unsigned char>(0xa5), static_cast<unsigned char>(0x5a)}) {
        const std::string context = "pattern " + std::to_string(pattern);
        std::memset(&parameters, pattern, 0x20);
        sceAudio3dGetDefaultOpenParameters(&parameters);
        RequireDefaults(&parameters, context);
        const auto* bytes = reinterpret_cast<const unsigned char*>(&parameters);
        for (std::size_t index = 0x20; index < sizeof(parameters); ++index) {
            RequireEqual(bytes[index], guardByte, context + ": extended byte " + std::to_string(index));
        }
    }
}};

} // namespace
