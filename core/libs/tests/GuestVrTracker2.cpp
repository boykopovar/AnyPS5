#include "prx/libSceVrTracker2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "ProcessParametersFixture.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {

}

namespace {

alignas(16) std::array<std::byte, 0x40> process{};

constexpr std::int32_t NotInitialized = static_cast<std::int32_t>(0x8A740001u);
constexpr std::int32_t InvalidArgument = static_cast<std::int32_t>(0x8A740006u);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "VrTracker2: %s\n", message);
        std::abort();
    }
}

std::array<std::uint8_t, 32> Filled() {
    std::array<std::uint8_t, 32> buffer{};
    buffer.fill(0xAA);
    return buffer;
}

bool AllEqual(const std::uint8_t* begin, const std::uint8_t* end, std::uint8_t value) {
    for (; begin != end; ++begin)
        if (*begin != value) return false;
    return true;
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

}

int main() {
    Require(VrTracker2NotInitialized() == NotInitialized, "plain export");
    Require(sceVrTracker2Finalize() == NotInitialized, "named export");

    auto wide = Filled();
    Require(VrTracker2ClearWideResult(wide.data()) == NotInitialized, "wide result return");
    Require(AllEqual(wide.data(), wide.data() + 24, 0) && AllEqual(wide.data() + 24, wide.data() + 32, 0xAA), "wide result clears 24 bytes");
    Require(VrTracker2ClearWideResult(nullptr) == InvalidArgument, "wide result null");

    auto narrow = Filled();
    Require(VrTracker2ClearNarrowResult(narrow.data()) == NotInitialized, "narrow result return");
    Require(AllEqual(narrow.data(), narrow.data() + 16, 0) && AllEqual(narrow.data() + 16, narrow.data() + 32, 0xAA), "narrow result clears 16 bytes");
    Require(VrTracker2ClearNarrowResult(nullptr) == InvalidArgument, "narrow result null");

    auto second = Filled();
    Require(VrTracker2CheckSecondThenReportNotInitialized(nullptr, second.data()) == NotInitialized, "second checked, first null");
    Require(VrTracker2CheckSecondThenReportNotInitialized(second.data(), nullptr) == InvalidArgument, "second null");
    Require(AllEqual(second.data(), second.data() + 32, 0xAA), "second untouched");

    Require(VrTracker2Accept() == 0, "accepting export");

    auto defaults = Filled();
    Require(VrTracker2FillDefaults(defaults.data()) == 0, "defaults return");
    const std::array<std::uint8_t, 16> expected{0xAA, 0xAA, 0xAA, 0xAA, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00};
    Require(std::memcmp(defaults.data(), expected.data(), expected.size()) == 0, "defaults layout");
    Require(AllEqual(defaults.data() + 16, defaults.data() + 32, 0xAA), "defaults stop at 16 bytes");
    Require(Throws([] { VrTracker2FillDefaults(nullptr); }), "defaults null");

    auto word = Filled();
    Require(VrTracker2ClearWord(word.data()) == 0, "clear word return");
    Require(AllEqual(word.data(), word.data() + 4, 0) && AllEqual(word.data() + 4, word.data() + 32, 0xAA), "clear word clears 4 bytes");
    Require(Throws([] { VrTracker2ClearWord(nullptr); }), "clear word null");

    std::array<std::uint8_t, 40> parameter{};
    parameter[0] = 0x20;
    parameter.back() = 0xAA;
    std::array<std::uint8_t, 40> requirement{};
    requirement.fill(0xAA);
    Require(sceVrTracker2QueryMemory(reinterpret_cast<const VrTracker2MemoryParameter*>(parameter.data()), reinterpret_cast<VrTracker2MemoryRequirement*>(requirement.data())) == 0, "query return");
    const std::array<std::uint8_t, 12> fields{0x20, 0, 0, 0, 0, 0, 0, 0x02, 0, 0, 0x01, 0};
    Require(std::memcmp(requirement.data(), fields.data(), fields.size()) == 0, "query fields");
    Require(AllEqual(requirement.data() + 12, requirement.data() + 32, 0) && AllEqual(requirement.data() + 32, requirement.data() + 40, 0xAA), "query writes 32 bytes");
    Require(sceVrTracker2QueryMemory(nullptr, reinterpret_cast<VrTracker2MemoryRequirement*>(requirement.data())) == InvalidArgument, "query null parameter");
    Require(sceVrTracker2QueryMemory(reinterpret_cast<const VrTracker2MemoryParameter*>(parameter.data()), nullptr) == InvalidArgument, "query null requirement");
    for (std::size_t offset = 4; offset < 32; ++offset) {
        auto rejected = parameter;
        rejected[offset] = 1;
        requirement.fill(0xAA);
        Require(sceVrTracker2QueryMemory(reinterpret_cast<const VrTracker2MemoryParameter*>(rejected.data()), reinterpret_cast<VrTracker2MemoryRequirement*>(requirement.data())) == InvalidArgument, "query reserved byte");
        Require(AllEqual(requirement.data(), requirement.data() + 40, 0xAA), "query rejection leaves output");
    }
    const std::uint64_t processSize = process.size();
    const std::uint32_t magic = 0x4942524f;
    std::memcpy(process.data(), &processSize, sizeof(processSize));
    std::memcpy(process.data() + 8, &magic, sizeof(magic));
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "process parameters");
    for (const std::uint32_t version : {0u, 0xffffffu, 0x1000000u, 0x7000000u}) {
        std::memcpy(process.data() + 0x10, &version, sizeof(version));
        for (const std::uint32_t mode : {1000u, 1001u}) {
            std::memcpy(parameter.data() + 4, &mode, sizeof(mode));
            requirement.fill(0xAA);
            const auto result = sceVrTracker2QueryMemory(reinterpret_cast<const VrTracker2MemoryParameter*>(parameter.data()), reinterpret_cast<VrTracker2MemoryRequirement*>(requirement.data()));
            if (version < 0x1000000) {
                Require(result == InvalidArgument, "query SDK restriction");
                Require(AllEqual(requirement.data(), requirement.data() + requirement.size(), 0xAA), "SDK rejection preserves output");
            } else {
                Require(result == 0, "query alternate mode");
                std::uint32_t memorySize;
                std::memcpy(&memorySize, requirement.data() + 4, sizeof(memorySize));
                Require(memorySize == (mode == 1000 ? 7u << 20 : 3u << 20), "query alternate memory size");
                Require(AllEqual(requirement.data() + 12, requirement.data() + 32, 0), "query alternate reserved fields");
                Require(AllEqual(requirement.data() + 32, requirement.data() + 40, 0xAA), "query alternate output boundary");
            }
        }
    }
    parameter[0] = 0x21;
    Require(sceVrTracker2QueryMemory(reinterpret_cast<const VrTracker2MemoryParameter*>(parameter.data()), reinterpret_cast<VrTracker2MemoryRequirement*>(requirement.data())) == InvalidArgument, "query size");

    Require(Throws([] { sceVrTracker2Initialize(nullptr); }), "unknown export");
    return 0;
}
