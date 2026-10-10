#include "ElfFixture.hpp"
#include "RelinkerProcess.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <source_location>
#include <string>

namespace {

using namespace RelinkerTests;
using namespace Testing;

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

Bytes ImageWithRelaTable(const std::uint64_t offset, const std::uint64_t size) {
    Bytes image = MakeExecutable(kCode);
    WriteTag(image, kRelaTag, kDtOsRela, offset);
    WriteTag(image, kRelaSzTag, kDtRelaSz, size);
    return image;
}

void RequireRejected(const Bytes& image, const std::string& what, std::source_location location = std::source_location::current()) {
    const TemporaryDirectory directory;
    const auto input = directory.Path() / "input.elf";
    const auto output = directory.Path() / "output.elf";
    WriteFile(input, image);

    const auto run = RunRelinker(RequireArgument(0, "relinker"), {"--skip-sce-module", "--to-intel", input.string(), output.string()},
                                 directory.Path() / "relinker.log");

    Require(run.ExitCode != 0, "Relinker accepted a relocation table " + what + " and exited 0:\n" + run.Output, location);
    Require(run.Output.find("Relocation table is out of bounds") != std::string::npos,
            "Relinker did not report the table as out of bounds for a table " + what + ":\n" + run.Output, location);
}

const Case offsetNearMaximum{"Relinker_RelaOffsetNearUint64Max_RejectsTable", [] {
    RequireRejected(ImageWithRelaTable(0xFFFFFFFFFFFFFFF8ull, 24), "whose offset is near UINT64_MAX");
}};

const Case tablePastEndOfFile{"Relinker_RelaTableExtendingPastEndOfFile_RejectsTable", [] {
    RequireRejected(ImageWithRelaTable(kImageSize - 16, 48), "that does not fit in the file");
}};

} // namespace
