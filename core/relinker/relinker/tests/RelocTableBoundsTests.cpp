#include "ElfFixture.hpp"
#include "RelinkerProcess.hpp"
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace RelinkerTests;

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

const Bytes kCode = {0xC3};

constexpr std::size_t kTagStride = 16;
constexpr std::size_t kTagBase = kCodeOffset + kDynamicVaddr;
constexpr std::size_t kEntryPointOffset = 24;
constexpr std::uint64_t kEntryVaddr = 0x100;
constexpr std::size_t kRelaTag = 4;
constexpr std::size_t kRelaSzTag = 5;
constexpr std::int64_t kDtRela = 7;
constexpr std::int64_t kDtRelaSz = 8;
constexpr std::int64_t kDtOsRela = 0x6100002f;
constexpr std::int64_t kDtOsRelaSz = 0x61000031;
constexpr std::uint64_t kImageSize = 0x8000;
constexpr std::uint64_t kUnmappedVaddr = 0x2000;
constexpr std::uint64_t kOffsetPastEnd = kImageSize + 0x1000;
constexpr std::uint64_t kOffsetBeforeEnd = kImageSize - 16;
constexpr std::size_t kRelaEntry = kCodeOffset + 0x700;
constexpr std::uint64_t kRelativeEntryInfo = 8;

struct OutputMode {
    std::string Flag;
    std::string Name;
};

const std::vector<OutputMode>& OutputModes() {
    static const std::vector<OutputMode> modes{{"--windows", "windows"}, {"--to-intel", "intel"}};
    return modes;
}

void WriteTag(Bytes& image, const std::size_t index, const std::int64_t tag, const std::uint64_t value) {
    Write<std::int64_t>(image, kTagBase + index * kTagStride, tag);
    Write<std::uint64_t>(image, kTagBase + index * kTagStride + 8, value);
}

void WriteRelativeEntry(Bytes& image) {
    Write<std::uint64_t>(image, kRelaEntry, 0x300);
    Write<std::uint64_t>(image, kRelaEntry + 8, kRelativeEntryInfo);
    Write<std::int64_t>(image, kRelaEntry + 16, 0);
}

Bytes ImageWithRelocationTable(const std::int64_t offsetTag, const std::int64_t sizeTag,
                               const std::uint64_t offset, const std::uint64_t size) {
    Bytes image = MakeExecutable(kCode);
    Write<std::uint64_t>(image, kEntryPointOffset, kEntryVaddr);
    Write<std::uint8_t>(image, kCodeOffset + kEntryVaddr, 0xC3);
    WriteTag(image, kRelaTag, offsetTag, offset);
    WriteTag(image, kRelaSzTag, sizeTag, size);
    return image;
}

void RunAndRequireAccepted(const std::string& binary, const Bytes& image, const OutputMode& mode, const std::string& what) {
    const TempDirectory directory;
    const auto input = directory.Path() / "input.elf";
    const auto output = directory.Path() / ("output." + mode.Name);
    WriteFile(input, image);
    const auto run = RunRelinker(binary, {"--skip-sce-module", mode.Flag, input.string(), output.string()}, directory.Path() / "relinker.log");
    require(run.ExitCode == 0 && std::filesystem::exists(output),
            "Relinker rejected " + what + " for " + mode.Flag + ":\n" + run.Output);
}

void RunAndRequireRejected(const std::string& binary, const Bytes& image, const OutputMode& mode, const std::string& what, const std::string& expected) {
    const TempDirectory directory;
    const auto input = directory.Path() / "input.elf";
    const auto output = directory.Path() / ("output." + mode.Name);
    WriteFile(input, image);
    const auto run = RunRelinker(binary, {"--skip-sce-module", mode.Flag, input.string(), output.string()}, directory.Path() / "relinker.log");
    require(run.ExitCode != 0 && !std::filesystem::exists(output),
            "Relinker accepted " + what + " for " + mode.Flag + ":\n" + run.Output);
    require(run.Output.find(expected) != std::string::npos,
            "Relinker did not report " + expected + " for " + what + ":\n" + run.Output);
}

}

int main(const int argc, char** argv) {
    try {
        require(argc == 2, "usage: reloc_table_bounds_tests <relinker>");
        const std::string binary = argv[1];
        const std::string tableError = "Relocation table is out of bounds";
        const std::string unmappedError = "Virtual address not mapped by any PT_LOAD segment";

        for (const auto& mode : OutputModes()) {
            RunAndRequireAccepted(binary, ImageWithRelocationTable(kDtRela, kDtRelaSz, 0x700, 0), mode, "an empty relocation table in the file");
            RunAndRequireAccepted(binary, ImageWithRelocationTable(kDtOsRela, kDtOsRelaSz, kRelaEntry, 0), mode, "an empty DT_OS_RELA table in the file");

            {
                Bytes image = ImageWithRelocationTable(kDtRela, kDtRelaSz, 0x700, 24);
                WriteRelativeEntry(image);
                RunAndRequireAccepted(binary, image, mode, "a relocation table that fits in the file");
            }

            RunAndRequireRejected(binary, ImageWithRelocationTable(kDtOsRela, kDtOsRelaSz, kOffsetPastEnd, 0), mode,
                                  "an empty relocation table whose offset is past the end of the file", tableError);
            RunAndRequireRejected(binary, ImageWithRelocationTable(kDtOsRela, kDtOsRelaSz, 0xFFFFFFFFFFFFFFFFull, 0), mode,
                                  "an empty relocation table whose offset is near UINT64_MAX", tableError);
            RunAndRequireRejected(binary, ImageWithRelocationTable(kDtOsRela, kDtOsRelaSz, kOffsetBeforeEnd, 0x108), mode,
                                  "a relocation table that does not fit in the file", tableError);
            RunAndRequireRejected(binary, ImageWithRelocationTable(kDtRela, kDtRelaSz, 0x700, 0xFFFFFFFFFFFFFFF0ull), mode,
                                  "a relocation table whose size does not fit in the file", tableError);
            RunAndRequireRejected(binary, ImageWithRelocationTable(kDtRela, kDtRelaSz, kUnmappedVaddr, 0), mode,
                                  "a relocation table whose address no segment maps", unmappedError);
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "reloc_table_bounds_tests: " << error.what() << '\n';
        return 1;
    }
}