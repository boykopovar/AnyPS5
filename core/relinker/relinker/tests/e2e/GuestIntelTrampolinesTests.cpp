#include "ElfFixture.hpp"
#include "RelinkerProcess.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

namespace {

using namespace RelinkerTests;
using namespace Testing;

const Bytes kMainCode = {0xC3};

const Bytes kModuleCode = {
    0x66, 0x48, 0x0F, 0x6E, 0xCF,
    0x66, 0x48, 0x0F, 0x6E, 0xD6,
    0x66, 0x0F, 0x78, 0xC1, 0x08, 0x08,
    0xF2, 0x0F, 0x78, 0xD1, 0x08, 0x10,
    0x66, 0x48, 0x0F, 0x7E, 0xD0,
    0xC3,
};
constexpr std::uint64_t kSites[] = {10, 16};
constexpr std::size_t kSiteLength = 6;

struct RelinkedModule {
    RelinkerRun Run;
    bool Written = false;
    Bytes Module;
};

std::uint64_t Expected(const std::uint64_t value, const std::uint64_t destination) {
    return (destination & ~0xFF0000ull) | (((value >> 8) & 0xFF) << 16);
}

RelinkedModule RelinkWithGuestModule() {
    const TemporaryDirectory directory;
    const auto input = directory.Path() / "input.elf";
    const auto output = directory.Path() / "output.elf";
    const auto modules = directory.Path() / "sce_module";
    std::filesystem::create_directory(modules);
    WriteFile(input, MakeExecutable(kMainCode));
    WriteFile(modules / "sample.prx", MakeModule(kModuleCode));
    RelinkedModule relinked;
    relinked.Run = RunRelinker(RequireArgument(0, "relinker"), {"--to-intel", input.string(), output.string()}, directory.Path() / "relinker.log");
    const auto module = directory.Path() / "app0" / "sce_module" / "sample.prx.guest.prx";
    relinked.Written = std::filesystem::exists(module);
    if (relinked.Written) relinked.Module = ReadFile(module);
    return relinked;
}

Bytes RequireConvertedModule() {
    auto relinked = RelinkWithGuestModule();
    Require(relinked.Run.ExitCode == 0, "Relinker failed:\n" + relinked.Run.Output);
    Require(relinked.Written, "The relinker did not write the converted guest module:\n" + relinked.Run.Output);
    return std::move(relinked.Module);
}

const Case relinkSucceeds{"Relinker_ToIntelWithSse4aGuestModule_ExitsSuccessfully", [] {
    const auto relinked = RelinkWithGuestModule();

    RequireEqual(relinked.Run.ExitCode, 0, "Relinker exit code, output:\n" + relinked.Run.Output);
}};

const Case moduleWritten{"Relinker_ToIntelWithSse4aGuestModule_WritesConvertedGuestModule", [] {
    const auto relinked = RelinkWithGuestModule();

    Require(relinked.Written, "The relinker did not write the converted guest module:\n" + relinked.Run.Output);
}};

const Case sitesBecomeJumps{"Relinker_GuestModuleSse4aSites_ReplacedByJumpsPaddedWithNops", [] {
    const MappedImage image(RequireConvertedModule());

    for (const auto site : kSites) {
        const auto* bytes = image.At<const std::uint8_t>(site);
        RequireEqual(bytes[0], std::uint8_t{0xE9}, "guest module SSE4a site " + std::to_string(site) + " opcode");
        for (std::size_t index = 5; index < kSiteLength; ++index)
            RequireEqual(bytes[index], std::uint8_t{0x90}, "guest module SSE4a site " + std::to_string(site) + " tail byte " + std::to_string(index));
    }
}};

const Case stubsOutsideCode{"Relinker_GuestModuleSse4aSites_JumpToStubsOutsideOriginalCode", [] {
    const MappedImage image(RequireConvertedModule());

    for (const auto site : kSites) {
        const auto* bytes = image.At<const std::uint8_t>(site);
        std::int32_t displacement;
        std::memcpy(&displacement, bytes + 1, sizeof(displacement));
        Require(site + 5 + displacement >= kCodeSize, "Guest module SSE4a stub for site " + std::to_string(site) + " is inside the original code segment");
    }
}};

const Case trampolinesCompute{"Relinker_GuestModuleTrampolines_ComputeOriginalResult", [] {
    const MappedImage image(RequireConvertedModule());
    const auto function = image.At<std::uint64_t(std::uint64_t, std::uint64_t)>(0);

    for (const auto [value, destination] : {std::pair{0x1122334455667788ull, 0x0123456789ABCDEFull}, {0ull, ~0ull}, {~0ull, 0ull},
                                            {0x9E3779B97F4A7C15ull, 0x0F1E2D3C4B5A6978ull}})
        RequireEqual(function(value, destination), Expected(value, destination), "Relinked guest module trampolines result");
}};

} // namespace
