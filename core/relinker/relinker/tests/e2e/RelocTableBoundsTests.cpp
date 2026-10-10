#include "ElfFixture.hpp"
#include "RelinkerProcess.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <filesystem>
#include <source_location>
#include <string>

namespace {

using namespace RelinkerTests;
using namespace Testing;

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

const std::string kTableError = "Relocation table is out of bounds";
const std::string kUnmappedError = "Virtual address not mapped by any PT_LOAD segment";

struct OutputMode {
    std::string Flag;
    std::string Name;
};

const OutputMode kWindows{"--windows", "windows"};
const OutputMode kIntel{"--to-intel", "intel"};

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

Bytes ImageWithFittingTable() {
    Bytes image = ImageWithRelocationTable(kDtRela, kDtRelaSz, 0x700, 24);
    WriteRelativeEntry(image);
    return image;
}

void RequireAccepted(const Bytes& image, const OutputMode& mode, const std::string& what,
                     std::source_location location = std::source_location::current()) {
    const TemporaryDirectory directory;
    const auto input = directory.Path() / "input.elf";
    const auto output = directory.Path() / ("output." + mode.Name);
    WriteFile(input, image);

    const auto run = RunRelinker(RequireArgument(0, "relinker"), {"--skip-sce-module", mode.Flag, input.string(), output.string()},
                                 directory.Path() / "relinker.log");

    Require(run.ExitCode == 0 && std::filesystem::exists(output), "Relinker rejected " + what + " for " + mode.Flag + ":\n" + run.Output, location);
}

void RequireRejected(const Bytes& image, const OutputMode& mode, const std::string& what, const std::string& expected,
                     std::source_location location = std::source_location::current()) {
    const TemporaryDirectory directory;
    const auto input = directory.Path() / "input.elf";
    const auto output = directory.Path() / ("output." + mode.Name);
    WriteFile(input, image);

    const auto run = RunRelinker(RequireArgument(0, "relinker"), {"--skip-sce-module", mode.Flag, input.string(), output.string()},
                                 directory.Path() / "relinker.log");

    Require(run.ExitCode != 0 && !std::filesystem::exists(output), "Relinker accepted " + what + " for " + mode.Flag + ":\n" + run.Output, location);
    Require(run.Output.find(expected) != std::string::npos, "Relinker did not report " + expected + " for " + what + ":\n" + run.Output, location);
}

void RequireEmptyTableAccepted(const OutputMode& mode, std::source_location location = std::source_location::current()) {
    RequireAccepted(ImageWithRelocationTable(kDtRela, kDtRelaSz, 0x700, 0), mode, "an empty relocation table in the file", location);
}

void RequireEmptyOsTableAccepted(const OutputMode& mode, std::source_location location = std::source_location::current()) {
    RequireAccepted(ImageWithRelocationTable(kDtOsRela, kDtOsRelaSz, kRelaEntry, 0), mode, "an empty DT_OS_RELA table in the file", location);
}

void RequireFittingTableAccepted(const OutputMode& mode, std::source_location location = std::source_location::current()) {
    RequireAccepted(ImageWithFittingTable(), mode, "a relocation table that fits in the file", location);
}

void RequireEmptyTablePastEndRejected(const OutputMode& mode, std::source_location location = std::source_location::current()) {
    RequireRejected(ImageWithRelocationTable(kDtOsRela, kDtOsRelaSz, kOffsetPastEnd, 0), mode,
                    "an empty relocation table whose offset is past the end of the file", kTableError, location);
}

void RequireEmptyTableNearMaximumRejected(const OutputMode& mode, std::source_location location = std::source_location::current()) {
    RequireRejected(ImageWithRelocationTable(kDtOsRela, kDtOsRelaSz, 0xFFFFFFFFFFFFFFFFull, 0), mode,
                    "an empty relocation table whose offset is near UINT64_MAX", kTableError, location);
}

void RequireTablePastEndRejected(const OutputMode& mode, std::source_location location = std::source_location::current()) {
    RequireRejected(ImageWithRelocationTable(kDtOsRela, kDtOsRelaSz, kOffsetBeforeEnd, 0x108), mode,
                    "a relocation table that does not fit in the file", kTableError, location);
}

void RequireHugeSizeRejected(const OutputMode& mode, std::source_location location = std::source_location::current()) {
    RequireRejected(ImageWithRelocationTable(kDtRela, kDtRelaSz, 0x700, 0xFFFFFFFFFFFFFFF0ull), mode,
                    "a relocation table whose size does not fit in the file", kTableError, location);
}

void RequireUnmappedTableRejected(const OutputMode& mode, std::source_location location = std::source_location::current()) {
    RequireRejected(ImageWithRelocationTable(kDtRela, kDtRelaSz, kUnmappedVaddr, 0), mode,
                    "a relocation table whose address no segment maps", kUnmappedError, location);
}

const Case windowsEmptyTable{"Relinker_WindowsEmptyRelaTableInFile_AcceptsTable", [] {
    RequireEmptyTableAccepted(kWindows);
}};

const Case intelEmptyTable{"Relinker_IntelEmptyRelaTableInFile_AcceptsTable", [] {
    RequireEmptyTableAccepted(kIntel);
}};

const Case windowsEmptyOsTable{"Relinker_WindowsEmptyOsRelaTableInFile_AcceptsTable", [] {
    RequireEmptyOsTableAccepted(kWindows);
}};

const Case intelEmptyOsTable{"Relinker_IntelEmptyOsRelaTableInFile_AcceptsTable", [] {
    RequireEmptyOsTableAccepted(kIntel);
}};

const Case windowsFittingTable{"Relinker_WindowsRelaTableFittingInFile_AcceptsTable", [] {
    RequireFittingTableAccepted(kWindows);
}};

const Case intelFittingTable{"Relinker_IntelRelaTableFittingInFile_AcceptsTable", [] {
    RequireFittingTableAccepted(kIntel);
}};

const Case windowsEmptyTablePastEnd{"Relinker_WindowsEmptyTableOffsetPastEndOfFile_RejectsTable", [] {
    RequireEmptyTablePastEndRejected(kWindows);
}};

const Case intelEmptyTablePastEnd{"Relinker_IntelEmptyTableOffsetPastEndOfFile_RejectsTable", [] {
    RequireEmptyTablePastEndRejected(kIntel);
}};

const Case windowsEmptyTableNearMaximum{"Relinker_WindowsEmptyTableOffsetNearUint64Max_RejectsTable", [] {
    RequireEmptyTableNearMaximumRejected(kWindows);
}};

const Case intelEmptyTableNearMaximum{"Relinker_IntelEmptyTableOffsetNearUint64Max_RejectsTable", [] {
    RequireEmptyTableNearMaximumRejected(kIntel);
}};

const Case windowsTablePastEnd{"Relinker_WindowsTableExtendingPastEndOfFile_RejectsTable", [] {
    RequireTablePastEndRejected(kWindows);
}};

const Case intelTablePastEnd{"Relinker_IntelTableExtendingPastEndOfFile_RejectsTable", [] {
    RequireTablePastEndRejected(kIntel);
}};

const Case windowsHugeSize{"Relinker_WindowsTableSizeNearUint64Max_RejectsTable", [] {
    RequireHugeSizeRejected(kWindows);
}};

const Case intelHugeSize{"Relinker_IntelTableSizeNearUint64Max_RejectsTable", [] {
    RequireHugeSizeRejected(kIntel);
}};

const Case windowsUnmappedTable{"Relinker_WindowsTableAtUnmappedAddress_RejectsTable", [] {
    RequireUnmappedTableRejected(kWindows);
}};

const Case intelUnmappedTable{"Relinker_IntelTableAtUnmappedAddress_RejectsTable", [] {
    RequireUnmappedTableRejected(kIntel);
}};

} // namespace
