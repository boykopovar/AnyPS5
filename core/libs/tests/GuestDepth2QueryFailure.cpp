#include "prx/libSceDepth2/libSceDepth2.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

struct GuardedMemory {
    std::uint64_t before;
    SceDepth2Memory memory;
    std::uint64_t after;
};

static_assert(sizeof(SceDepth2Memory) == 16);
static_assert(offsetof(GuardedMemory, memory) == sizeof(std::uint64_t));
static_assert(offsetof(GuardedMemory, after) == offsetof(GuardedMemory, memory) + sizeof(SceDepth2Memory));

constexpr GuardedMemory initialMemory{
    0x13579BDF2468ACEFull,
    {0x0123456789ABCDEFull, 0xFEDCBA9876543210ull},
    0xF1E2D3C4B5A69788ull,
};

int Check(bool value, const char* name, const char* message) {
    if (value) return 0;
    std::fprintf(stderr, "%s: %s\n", name, message);
    return 1;
}

int CheckResult(int actual, std::uint32_t expected, const char* name) {
    if (static_cast<std::uint32_t>(actual) == expected) return 0;
    std::fprintf(stderr, "%s: expected error 0x%08x, got 0x%08x\n", name,
        static_cast<unsigned int>(expected), static_cast<unsigned int>(actual));
    return 1;
}

int CheckGuards(const GuardedMemory& output, const char* name) {
    int failures = Check(output.before == initialMemory.before, name, "leading guard changed");
    failures += Check(output.after == initialMemory.after, name, "trailing guard changed");
    return failures;
}

int CheckFailure(const SceDepth2Param* param, std::uint32_t expected, const char* name) {
    GuardedMemory output = initialMemory;
    int failures = CheckResult(sceDepth2QueryMemory(param, &output.memory), expected, name);
    failures += Check(std::memcmp(&output.memory, &initialMemory.memory, sizeof(SceDepth2Memory)) == 0,
        name, "output bytes changed");
    failures += CheckGuards(output, name);
    return failures;
}

int CheckSuccess(const SceDepth2Param& param, const char* name) {
    GuardedMemory output = initialMemory;
    int failures = CheckResult(sceDepth2QueryMemory(&param, &output.memory), 0, name);
    failures += Check(output.memory.size == 0xC00000, name, "incorrect successful size");
    failures += Check(output.memory.alignment == 0x40000, name, "incorrect successful alignment");
    failures += CheckGuards(output, name);
    return failures;
}

struct FailureCase {
    const char* name;
    bool mode1;
    std::uint32_t SceDepth2Param::* field;
    std::uint32_t value;
    std::uint32_t error;
};

}

int main() {
    SceDepth2Param baselineA{};
    baselineA.size = 0x40;
    baselineA.inputWidth = 320;
    baselineA.inputHeight = 200;
    baselineA.outputWidth = 320;
    baselineA.outputHeight = 200;

    SceDepth2Param baselineB = baselineA;
    baselineB.mode = 1;
    baselineB.inputWidth = 480;
    baselineB.inputHeight = 270;
    baselineB.outputWidth = 480;
    baselineB.outputHeight = 270;

    const std::array<FailureCase, 15> cases{{
        {"size", false, &SceDepth2Param::size, 0x30, 0x80BB0008u},
        {"type", false, &SceDepth2Param::type, 1, 0x80BB0012u},
        {"mode", false, &SceDepth2Param::mode, 2, 0x80BB000Bu},
        {"mode 0 input width", false, &SceDepth2Param::inputWidth, 321, 0x80BB000Bu},
        {"mode 0 input height", false, &SceDepth2Param::inputHeight, 201, 0x80BB000Bu},
        {"mode 1 input width", true, &SceDepth2Param::inputWidth, 481, 0x80BB000Cu},
        {"mode 1 input height", true, &SceDepth2Param::inputHeight, 271, 0x80BB000Cu},
        {"input property", false, &SceDepth2Param::unknown0c, 2, 0x80BB0003u},
        {"output width", false, &SceDepth2Param::outputWidth, 321, 0x80BB000Bu},
        {"output height", false, &SceDepth2Param::outputHeight, 201, 0x80BB000Bu},
        {"output property", false, &SceDepth2Param::unknown18, 2, 0x80BB0002u},
        {"reserved20", false, &SceDepth2Param::reserved20, 1, 0x80BB0009u},
        {"reserved24", false, &SceDepth2Param::reserved24, 1, 0x80BB0009u},
        {"unknown30", false, &SceDepth2Param::unknown30, 7, 0x80BB0000u},
        {"unknown34", false, &SceDepth2Param::unknown34, 8, 0x80BB0000u},
    }};

    int failures = CheckSuccess(baselineA, "mode 0 success");
    failures += CheckSuccess(baselineB, "mode 1 success");
    for (const FailureCase& test : cases) {
        SceDepth2Param param = test.mode1 ? baselineB : baselineA;
        param.*(test.field) = test.value;
        failures += CheckFailure(&param, test.error, test.name);
    }
    failures += CheckFailure(nullptr, 0x80BB000Au, "null parameter");
    failures += CheckResult(sceDepth2QueryMemory(&baselineA, nullptr), 0x80BB000Au, "null output");
    return failures == 0 ? 0 : 1;
}
