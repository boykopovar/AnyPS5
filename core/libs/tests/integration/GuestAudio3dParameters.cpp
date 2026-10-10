#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" void APS5_VABI sceAudio3dGetDefaultOpenParameters(Audio3dOpenParameters* parameters);

static_assert(sizeof(Audio3dOpenParameters) == 0x28);
static_assert(offsetof(Audio3dOpenParameters, num_beds) == 0x20);

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

static void CheckDefaults(const void* memory) {
    Audio3dOpenParameters parameters{};
    std::memcpy(&parameters, memory, 0x20);
    Require(parameters.size_this == 0x20, "default parameter size must be 32 bytes");
    Require(parameters.granularity == 256, "incorrect default granularity");
    Require(parameters.rate == 0, "incorrect default rate");
    Require(parameters.max_objects == 512, "incorrect default object limit");
    Require(parameters.queue_depth == 2, "incorrect default queue depth");
    Require(parameters.buffer_mode == 2, "incorrect default buffer mode");
    Require(parameters.pad == 0, "reserved parameter bytes must be cleared");
}

static void CheckInvalidDestination() {
    bool threw = false;
    try {
        sceAudio3dGetDefaultOpenParameters(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "null destination must throw invalid_argument");
}

static void CheckShortBuffer() {
    struct GuardedParameters {
        std::array<unsigned char, 8> before;
        alignas(Audio3dOpenParameters) std::array<unsigned char, 0x20> parameters;
        std::array<unsigned char, 8> after;
    } guarded;
    static_assert(offsetof(GuardedParameters, after) == offsetof(GuardedParameters, parameters) + 0x20);
    std::memset(&guarded, 0xa5, sizeof(guarded));
    for (std::uint64_t size : std::array<std::uint64_t, 6>{0, 0x10, 0x18, 0x20, 0x28, UINT64_MAX}) {
        guarded.parameters.fill(0xa5);
        std::memcpy(guarded.parameters.data(), &size, sizeof(size));
        sceAudio3dGetDefaultOpenParameters(reinterpret_cast<Audio3dOpenParameters*>(guarded.parameters.data()));
        CheckDefaults(guarded.parameters.data());
        for (unsigned char value : guarded.before) Require(value == 0xa5, "parameter underrun");
        for (unsigned char value : guarded.after) Require(value == 0xa5, "parameter overrun");
    }
}

static void CheckExtendedBuffer() {
    Audio3dOpenParameters parameters;
    std::memset(&parameters, 0xa5, sizeof(parameters));
    for (unsigned char pattern : {0xa5, 0x5a}) {
        std::memset(&parameters, pattern, 0x20);
        sceAudio3dGetDefaultOpenParameters(&parameters);
        CheckDefaults(&parameters);
        const auto* bytes = reinterpret_cast<const unsigned char*>(&parameters);
        for (std::size_t i = 0x20; i < sizeof(parameters); ++i) {
            Require(bytes[i] == 0xa5, "extended parameter bytes must remain unchanged");
        }
    }
}

int main() {
    CheckInvalidDestination();
    CheckShortBuffer();
    CheckExtendedBuffer();
}
