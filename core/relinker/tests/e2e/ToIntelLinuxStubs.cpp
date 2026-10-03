#include "ElfFixture.hpp"
#include "RelinkerProcess.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using namespace RelinkerTests;

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

const Bytes kCode = {
    0x66, 0x48, 0x0F, 0x6E, 0xCF,
    0x66, 0x48, 0x0F, 0x6E, 0xD6,
    0x66, 0x0F, 0x78, 0xC1, 0x08, 0x08,
    0xF2, 0x0F, 0x78, 0xD1, 0x08, 0x10,
    0x66, 0x48, 0x0F, 0x7E, 0xD0,
    0xC3,
};
constexpr std::uint64_t kSites[] = {10, 16};
constexpr std::size_t kSiteLength = 6;

std::uint64_t expected(const std::uint64_t value, const std::uint64_t destination) {
    return (destination & ~0xFF0000ull) | (((value >> 8) & 0xFF) << 16);
}

}

int main(const int argc, char** argv) {
    try {
        require(argc == 2, "usage: ToIntelLinuxStubs <relinker>");
        const TempDirectory directory;
        const auto input = directory.Path() / "input.elf";
        const auto output = directory.Path() / "output.elf";
        WriteFile(input, MakeExecutable(kCode));
        const auto run = RunRelinker(argv[1], {"--skip-sce-module", "--to-intel", input.string(), output.string()}, directory.Path() / "relinker.log");
        require(run.ExitCode == 0, "Relinker failed:\n" + run.Output);
        require(run.Output.find("Intel conversion: 0 in place, 2 stubs") != std::string::npos, "Relinker did not report two stubs:\n" + run.Output);

        const MappedImage image(ReadFile(output));
        for (const auto site : kSites) {
            const auto* bytes = image.At<const std::uint8_t>(site);
            require(bytes[0] == 0xE9, "SSE4a site was not replaced by a jump");
            for (std::size_t index = 5; index < kSiteLength; ++index) require(bytes[index] == 0x90, "SSE4a site tail is not padded with NOPs");
            std::int32_t displacement;
            std::memcpy(&displacement, bytes + 1, sizeof(displacement));
            require(site + 5 + displacement >= kCodeSize, "SSE4a stub is inside the original code segment");
        }

        const auto function = image.At<std::uint64_t(std::uint64_t, std::uint64_t)>(0);
        for (const auto [value, destination] : {std::pair{0x1122334455667788ull, 0x0123456789ABCDEFull}, {0ull, ~0ull}, {~0ull, 0ull}, {0x9E3779B97F4A7C15ull, 0x0F1E2D3C4B5A6978ull}})
            require(function(value, destination) == expected(value, destination), "Relinked SSE4a stubs computed the wrong value");
        std::cout << "--to-intel Linux stub tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
