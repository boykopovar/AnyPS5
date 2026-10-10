#include "ElfFixture.hpp"
#include "RelinkerProcess.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace RelinkerTests;

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

const Bytes kCode = {0xC3};

constexpr std::size_t kTagStride = 16;
constexpr std::size_t kTagBase = kCodeOffset + kDynamicVaddr;
constexpr std::size_t kRelaTag = 4;
constexpr std::size_t kRelaSzTag = 5;
constexpr std::int64_t kDtOsRela = 0x6100002f;
constexpr std::int64_t kDtRelaSz = 8;
constexpr std::size_t kImageSize = 0x8000;

void WriteTag(Bytes& image, const std::size_t index, const std::int64_t tag, const std::uint64_t value) {
    Write<std::int64_t>(image, kTagBase + index * kTagStride, tag);
    Write<std::uint64_t>(image, kTagBase + index * kTagStride + 8, value);
}

void RunAndRequireRejected(const std::string& binary, const Bytes& image, const std::string& what) {
    const TempDirectory directory;
    const auto input = directory.Path() / "input.elf";
    const auto output = directory.Path() / "output.elf";
    WriteFile(input, image);
    const auto run = RunRelinker(binary, {"--skip-sce-module", "--to-intel", input.string(), output.string()}, directory.Path() / "relinker.log");
    require(run.ExitCode != 0, "Relinker accepted a relocation table " + what + " and exited 0:\n" + run.Output);
    require(run.Output.find("Relocation table is out of bounds") != std::string::npos,
            "Relinker did not report the table as out of bounds for a table " + what + ":\n" + run.Output);
}

}

int main(const int argc, char** argv) {
    try {
        require(argc == 2, "usage: rela_bounds_tests <relinker>");
        const std::string binary = argv[1];

        {
            Bytes image = MakeExecutable(kCode);
            WriteTag(image, kRelaTag, kDtOsRela, 0xFFFFFFFFFFFFFFF8ull);
            WriteTag(image, kRelaSzTag, kDtRelaSz, 24);
            RunAndRequireRejected(binary, image, "whose offset is near UINT64_MAX");
        }

        {
            Bytes image = MakeExecutable(kCode);
            WriteTag(image, kRelaTag, kDtOsRela, kImageSize - 16);
            WriteTag(image, kRelaSzTag, kDtRelaSz, 48);
            RunAndRequireRejected(binary, image, "that does not fit in the file");
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rela_bounds_tests: " << error.what() << '\n';
        return 1;
    }
}
