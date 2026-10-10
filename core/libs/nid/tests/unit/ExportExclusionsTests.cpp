#include "PeImageBuilder.hpp"

#include <Testing/Test.hpp>
#include <nid/ExportExclusions.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using namespace NidTests;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

std::string WriteFile(const Testing::TemporaryDirectory& directory, const std::vector<std::uint8_t>& bytes) {
    const auto path = directory.Path() / "reference.bin";
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path.string();
}

void RequireReadFails(const std::vector<std::uint8_t>& bytes, const std::string& expected) {
    const Testing::TemporaryDirectory directory;
    const auto path = WriteFile(directory, bytes);
    Testing::RequireThrowsWithMessage<std::runtime_error>([&] { Nid::ReadExportExclusions(path); }, expected, "reference rejected");
}

const Case peReference{"ReadExportExclusions_PeReference_ReturnsNormalizedExportNames", [] {
    PeImageBuilder builder;
    builder.Exports = {{"printf_nid_postfix", 0}, {"sceKernelUsleep", 1}};
    const Testing::TemporaryDirectory directory;
    const auto exclusions = Nid::ReadExportExclusions(WriteFile(directory, builder.Build()));
    RequireEqual(exclusions.size(), std::size_t{2}, "export count");
    Require(exclusions.contains("printf"), "postfix normalized away");
    Require(exclusions.contains("sceKernelUsleep"), "plain export kept");
}};

const Case missingFile{"ReadExportExclusions_MissingFile_Throws", [] {
    const Testing::TemporaryDirectory directory;
    const auto path = (directory.Path() / "absent.prx").string();
    Testing::RequireThrowsWithMessage<std::runtime_error>(
        [&] { Nid::ReadExportExclusions(path); }, "cannot open export reference: " + path, "missing file");
}};

const Case emptyFile{"ReadExportExclusions_EmptyFile_Throws", [] {
    const Testing::TemporaryDirectory directory;
    const auto path = WriteFile(directory, {});
    Testing::RequireThrowsWithMessage<std::runtime_error>(
        [&] { Nid::ReadExportExclusions(path); }, "empty or unreadable export reference: " + path, "empty file");
}};

const Case unknownFormat{"ReadExportExclusions_UnknownFormat_Throws", [] {
    const Testing::TemporaryDirectory directory;
    const auto path = WriteFile(directory, {'#', '!', 's', 'h'});
    Testing::RequireThrowsWithMessage<std::runtime_error>(
        [&] { Nid::ReadExportExclusions(path); }, "unrecognized export reference format: " + path, "unknown format");
}};

const Case badSignature{"ReadExportExclusions_PeWithBadSignature_Throws", [] {
    PeImageBuilder builder;
    builder.Exports = {{"printf", 0}};
    auto image = builder.Build();
    image[PeImageLayout::PeHeader] = 'X';
    RequireReadFails(image, "invalid reference PE signature");
}};

const Case noNamedExports{"ReadExportExclusions_PeWithoutNamedExports_Throws", [] {
    RequireReadFails(PeImageBuilder().Build(), "reference PE has no named exports");
}};

const Case duplicateExport{"ReadExportExclusions_PeWithDuplicateExport_Throws", [] {
    PeImageBuilder builder;
    builder.Exports = {{"printf", 0}, {"printf", 1}};
    RequireReadFails(builder.Build(), "duplicate reference export: printf");
}};

const Case unmappedNames{"ReadExportExclusions_NamesArrayOutsideRawData_Throws", [] {
    PeImageBuilder builder;
    builder.Exports = {{"printf", 0}};
    builder.RawSize = PeImageBuilder::NamesArray(1);
    RequireReadFails(builder.Build(), "unmapped reference PE export RVA");
}};

const Case missingDataDirectories{"ReadExportExclusions_ZeroDataDirectoryCount_Throws", [] {
    PeImageBuilder builder;
    builder.Exports = {{"printf", 0}};
    auto image = builder.Build();
    Nid::Internal::Write<std::uint32_t>(image, PeImageBuilder::DataDirectories(builder.Magic) - 4, 0u);
    RequireReadFails(image, "missing reference PE data directories");
}};

} // namespace
