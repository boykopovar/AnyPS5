#include "prx/libSceVrTracker2/include/Api.hpp"
#include "prx/libkernel/libkernel.hpp"
#include "ProcessParametersFixture.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

static_assert(sizeof(VrTracker2InitializeParameter) == 72);

namespace {

alignas(16) std::array<std::byte, 0x40> process{};

constexpr std::int32_t NotInitialized = static_cast<std::int32_t>(0x8A740001u);
constexpr std::int32_t InvalidArgument = static_cast<std::int32_t>(0x8A740006u);
constexpr std::uint32_t TrackerMemory = 32u << 20;
constexpr std::size_t WorkMemoryTail = 64u << 10;

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

VrTracker2InitializeParameter TrackerParameter(std::vector<std::uint8_t>& memory) {
    VrTracker2InitializeParameter parameter{};
    parameter.size = sizeof(parameter);
    parameter.threadPriorities[0] = 256;
    parameter.threadPriorities[1] = 767;
    parameter.threadAffinityMasks[0] = 0x1fff;
    parameter.workMemory = memory.data();
    parameter.workMemorySize = static_cast<std::uint32_t>(memory.size());
    parameter.workMemoryAlignment = 0x10000;
    return parameter;
}

bool WorkMemoryCleared(const std::vector<std::uint8_t>& memory) {
    return AllEqual(memory.data(), memory.data() + TrackerMemory, 0) && AllEqual(memory.data() + TrackerMemory, memory.data() + memory.size(), 0xAA);
}

bool WorkMemoryUntouched(const std::vector<std::uint8_t>& memory) {
    return AllEqual(memory.data(), memory.data() + memory.size(), 0xAA);
}

std::int32_t InitializeFresh(std::vector<std::uint8_t>& memory, const VrTracker2InitializeParameter& parameter) {
    std::fill(memory.begin(), memory.end(), 0xAA);
    return sceVrTracker2Initialize(&parameter);
}

void CheckInitializeValidation(std::vector<std::uint8_t>& memory) {
    const auto valid = TrackerParameter(memory);
    Require(sceVrTracker2Initialize(nullptr) == InvalidArgument, "initialize null");
    for (const std::uint32_t size : {0u, 71u, 73u}) {
        auto parameter = valid;
        parameter.size = size;
        parameter.mode = 1000;
        Require(InitializeFresh(memory, parameter) == InvalidArgument, "initialize size before mode");
    }
    for (const std::uint32_t mode : {1u, 999u, 1002u, 0xffffffffu}) {
        auto parameter = valid;
        parameter.mode = mode;
        Require(InitializeFresh(memory, parameter) == InvalidArgument, "initialize mode");
    }
    auto parameter = valid;
    parameter.workMemory = nullptr;
    Require(InitializeFresh(memory, parameter) == InvalidArgument, "initialize work memory null");
    parameter = valid;
    parameter.workMemorySize = TrackerMemory - 1;
    Require(InitializeFresh(memory, parameter) == InvalidArgument, "initialize work memory size");
    parameter = valid;
    parameter.workMemoryAlignment = 0xffff;
    Require(InitializeFresh(memory, parameter) == InvalidArgument, "initialize alignment");
    for (std::size_t index = 0; index < sizeof(parameter.reserved); ++index) {
        parameter = valid;
        parameter.reserved[index] = 1;
        Require(InitializeFresh(memory, parameter) == InvalidArgument, "initialize reserved byte");
    }
    for (std::size_t index = 0; index < 2; ++index) {
        for (const std::int32_t priority : {255, 768, -1}) {
            parameter = valid;
            parameter.threadPriorities[index] = priority;
            Require(InitializeFresh(memory, parameter) == InvalidArgument, "initialize thread priority");
        }
        for (const std::uint64_t mask : {std::uint64_t{0x2000}, std::uint64_t{0x3fff}, std::uint64_t{1} << 63}) {
            parameter = valid;
            parameter.threadAffinityMasks[index] = mask;
            Require(InitializeFresh(memory, parameter) == InvalidArgument, "initialize affinity mask");
        }
    }
    Require(WorkMemoryUntouched(memory), "initialize validation leaves work memory");
}

void CheckInitializeTracking(std::vector<std::uint8_t>& memory, std::uint32_t sdkVersion) {
    const auto valid = TrackerParameter(memory);
    Require(InitializeFresh(memory, valid) == -1, "initialize tracking start result");
    Require(WorkMemoryCleared(memory), "initialize clears tracker memory");
    Require(sceVrTracker2Finalize() == NotInitialized, "initialize failure leaves module uninitialized");
    Require(VrTracker2NotInitialized() == NotInitialized, "initialize failure keeps uninitialized answers");
    Require(InitializeFresh(memory, valid) == -1, "initialize retry after failure");
    auto parameter = valid;
    parameter.workMemoryAlignment = 0x10001;
    parameter.option = 0xffffffffu;
    parameter.coordinateSystem = 2;
    parameter.disableOption = 1;
    parameter.threadAffinityMasks[1] = 0x1fff;
    Require(InitializeFresh(memory, parameter) == -1, "initialize late checks follow tracking");
    for (const std::int8_t optionMode : {std::int8_t{-1}, std::int8_t{0}}) {
        parameter = valid;
        parameter.optionMode = optionMode;
        Require(InitializeFresh(memory, parameter) == -1, "initialize accepted option mode");
    }
    for (const std::int8_t optionMode : {std::int8_t{2}, std::int8_t{-2}, std::int8_t{127}}) {
        parameter = valid;
        parameter.optionMode = optionMode;
        Require(InitializeFresh(memory, parameter) == InvalidArgument, "initialize option mode");
        Require(WorkMemoryCleared(memory), "initialize option mode follows clearing");
    }
    parameter = valid;
    parameter.optionMode = 1;
    Require(InitializeFresh(memory, parameter) == (sdkVersion >= 0x7000000 ? -1 : InvalidArgument), "initialize SDK option mode");
    Require(sceVrTracker2Finalize() == NotInitialized, "initialize leaves module uninitialized");
}

std::filesystem::path EnterApplicationRoot() {
    std::random_device device;
    std::filesystem::path root;
    do {
        root = std::filesystem::temp_directory_path() /
            ("guest_vr_tracker2_" + std::to_string(getpid_nid_postfix()) + "_" + std::to_string(device()));
    } while (!std::filesystem::create_directory(root));
    std::filesystem::create_directories(root / "app0" / "sce_sys");
    std::ofstream(root / "app0" / "sce_sys" / "param.json") << "{\n  \"contentId\": \"IV0000-ABCD00000_00-VRTRACKER2000000\",\n  \"localizedParameters\": {\"defaultLanguage\": \"en-US\", \"en-US\": {\"titleName\": \"VrTracker2\"}},\n  \"titleId\": \"ABCD00000\"\n}\n";
    std::filesystem::current_path(root);
    return root;
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

    std::vector<std::uint8_t> memory(TrackerMemory + WorkMemoryTail);
    CheckInitializeValidation(memory);
    const auto previous = std::filesystem::current_path();
    const auto root = EnterApplicationRoot();
    for (const std::uint32_t version : {0x6ffffffu, 0x7000000u}) {
        std::memcpy(process.data() + 0x10, &version, sizeof(version));
        CheckInitializeTracking(memory, version);
    }
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    return 0;
}
