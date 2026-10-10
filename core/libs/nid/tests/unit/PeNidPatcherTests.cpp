#include "PeImageBuilder.hpp"

#include <Testing/Test.hpp>
#include <nid/PeNidPatcher.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace NidTests;
using Nid::Internal::Read;
using Nid::Internal::ReadCStr;
using Nid::Internal::Write;
using Testing::Case;
using Testing::RequireEqual;

const std::string printfNid = "hcuQgD53UxM";
const std::string usleepNid = "1jfXLRVzisc";

void Patch(std::vector<std::uint8_t>& image) {
    Nid::PeNidPatcher().PatchNids(image, "libTest", {});
}

void RequirePatchFails(std::vector<std::uint8_t> image, const std::string& expected) {
    Testing::RequireThrowsWithMessage<std::runtime_error>([&] { Patch(image); }, expected, "patch rejects image");
}

PeImageBuilder ThreeExports() {
    PeImageBuilder builder;
    builder.Exports = {{"printf_nid_postfix", 2}, {"sceKernelUsleep", 0}, {"helper_nid_no_patch", 1}};
    return builder;
}

std::string ImportName(const std::vector<std::uint8_t>& image, std::uint32_t nameOffset) {
    return ReadCStr(image, PeImageBuilder::FileOffset(nameOffset + 2u));
}

const Case exportsRenamed{"PatchNids_Exports_ReplacesNamesWithNids", [] {
    auto image = ThreeExports().Build();
    Patch(image);
    RequireEqual(ExportNames(image, 3), std::vector<std::string>{usleepNid, printfNid, "helper_nid_no_patch"}, "patched names");
}};

const Case exportsSorted{"PatchNids_Exports_SortsNamesAndKeepsOrdinalPairs", [] {
    auto image = ThreeExports().Build();
    Patch(image);
    const auto names = ExportNames(image, 3);
    const auto ordinals = ExportOrdinals(image, 3);
    Testing::Require(std::is_sorted(names.begin(), names.end()), "names sorted for binary search");
    RequireEqual(ordinals, std::vector<std::uint16_t>{0, 2, 1}, "ordinals follow their names");
}};

const Case exportsRepackedFromFirstString{"PatchNids_Exports_RepacksStringsFromOriginalRegionStart", [] {
    auto image = ThreeExports().Build();
    Patch(image);
    std::vector<std::uint32_t> rvas;
    for (std::size_t index = 0; index < 3; ++index) {
        rvas.push_back(Read<std::uint32_t>(image, PeImageBuilder::FileOffset(PeImageBuilder::NamesArray(3)) + index * 4u));
    }
    std::sort(rvas.begin(), rvas.end());
    const auto start = PeImageLayout::SectionRva + PeImageLayout::ExportStrings;
    RequireEqual(rvas, std::vector<std::uint32_t>{start, start + 12u, start + 24u}, "nids packed back to back from region start");
}};

const Case virtualSizeGrows{"PatchNids_LongerNames_GrowsSectionVirtualSize", [] {
    PeImageBuilder builder;
    builder.Exports = {{"sceA", 0}};
    builder.VirtualSize = PeImageLayout::ExportStrings + 5u;
    auto image = builder.Build();
    Patch(image);
    const auto virtualSize = Read<std::uint32_t>(image, PeImageBuilder::SectionTable(builder.Magic) + 8);
    RequireEqual(virtualSize, PeImageLayout::ExportStrings + 12u, "virtual size covers repacked nid");
}};

const Case importsRenamed{"PatchNids_SceAndPostfixImports_ReplacesNamesWithNids", [] {
    auto builder = ThreeExports();
    builder.Imports = {{"sceKernelUsleep"}, {"memcpy"}, {"printf_nid_postfix"}};
    auto image = builder.Build();
    Patch(image);
    RequireEqual(ImportName(image, PeImageLayout::ImportNames), usleepNid, "sce import");
    RequireEqual(ImportName(image, PeImageLayout::ImportNames + 18u), std::string("memcpy"), "plain import untouched");
    RequireEqual(ImportName(image, PeImageLayout::ImportNames + 27u), printfNid, "postfix import");
}};

const Case importsByOrdinalSkipped{"PatchNids_ImportByOrdinal_IsLeftUntouched", [] {
    auto builder = ThreeExports();
    builder.Imports = {{"", true}, {"sceKernelUsleep"}};
    auto image = builder.Build();
    const auto thunk = Read<std::uint64_t>(image, PeImageBuilder::FileOffset(PeImageLayout::ImportThunks));
    Patch(image);
    RequireEqual(Read<std::uint64_t>(image, PeImageBuilder::FileOffset(PeImageLayout::ImportThunks)), thunk, "ordinal thunk");
    RequireEqual(ImportName(image, PeImageLayout::ImportNames), usleepNid, "named import after ordinal");
}};

const Case importsPe32{"PatchNids_Pe32Image_PatchesExportsAndFourByteThunks", [] {
    auto builder = ThreeExports();
    builder.Magic = 0x10b;
    builder.Imports = {{"sceKernelUsleep"}};
    auto image = builder.Build();
    Patch(image);
    RequireEqual(ExportNames(image, 3).front(), usleepNid, "PE32 export");
    RequireEqual(ImportName(image, PeImageLayout::ImportNames), usleepNid, "PE32 import");
}};

const Case importTooShort{"PatchNids_ImportShorterThanNid_Throws", [] {
    auto builder = ThreeExports();
    builder.Imports = {{"sceA"}};
    RequirePatchFails(builder.Build(), "import NID longer than original name: sceA");
}};

const Case ehFrameRenamed{"PatchNids_LongEhFrameSectionName_IsShortenedForPe", [] {
    auto builder = ThreeExports();
    builder.EhFrameSection = true;
    auto image = builder.Build();
    Patch(image);
    const auto name = std::string(reinterpret_cast<const char*>(image.data() + PeImageBuilder::SectionTable(builder.Magic) + 40));
    RequireEqual(name, std::string(".ehfram"), "section name");
}};

const Case tooSmall{"PatchNids_FileSmallerThanDosHeader_Throws", [] {
    RequirePatchFails(std::vector<std::uint8_t>(0x3f), "file too small");
}};

const Case headerOffsetPastEnd{"PatchNids_PeHeaderOffsetPastEnd_Throws", [] {
    auto image = ThreeExports().Build();
    Write<std::uint32_t>(image, 0x3c, static_cast<std::uint32_t>(image.size() - 3));
    RequirePatchFails(image, "invalid pe header offset");
}};

const Case badSignature{"PatchNids_BadPeSignature_Throws", [] {
    auto image = ThreeExports().Build();
    image[PeImageLayout::PeHeader + 1] = 'X';
    RequirePatchFails(image, "not a PE file");
}};

const Case noOptionalHeader{"PatchNids_ZeroOptionalHeaderSize_Throws", [] {
    auto image = ThreeExports().Build();
    Write<std::uint16_t>(image, PeImageLayout::Coff + 16, 0);
    RequirePatchFails(image, "no optional header");
}};

const Case badMagic{"PatchNids_UnknownOptionalHeaderMagic_Throws", [] {
    auto image = ThreeExports().Build();
    Write<std::uint16_t>(image, PeImageLayout::Optional, 0x107);
    RequirePatchFails(image, "unsupported optional header magic");
}};

const Case noExportDirectory{"PatchNids_MissingExportDirectory_Throws", [] {
    auto image = ThreeExports().Build();
    Write<std::uint32_t>(image, PeImageBuilder::DataDirectories(0x20b), 0);
    RequirePatchFails(image, "no export directory");
}};

const Case unmappedExportDirectory{"PatchNids_ExportDirectoryOutsideSections_Throws", [] {
    auto image = ThreeExports().Build();
    Write<std::uint32_t>(image, PeImageBuilder::DataDirectories(0x20b), 0x9000);
    RequirePatchFails(image, "rva not mapped to any section");
}};

const Case sectionTablePastEnd{"PatchNids_SectionCountPastEndOfFile_Throws", [] {
    auto image = ThreeExports().Build();
    Write<std::uint16_t>(image, PeImageLayout::Coff + 2, 0x400);
    RequirePatchFails(image, "read out of bounds");
}};

const Case noNames{"PatchNids_ExportDirectoryWithoutNames_Throws", [] {
    PeImageBuilder builder;
    RequirePatchFails(builder.Build(), "no exported names");
}};

const Case emptyName{"PatchNids_EmptyExportName_Throws", [] {
    PeImageBuilder builder;
    builder.Exports = {{"", 0}};
    RequirePatchFails(builder.Build(), "empty exported name");
}};

const Case noRawSpace{"PatchNids_NidsLargerThanRawSection_Throws", [] {
    PeImageBuilder builder;
    builder.Exports = {{"a", 0}, {"b", 1}};
    builder.FileAlignment = 1;
    builder.RawSize = PeImageLayout::ExportStrings + 4u;
    RequirePatchFails(builder.Build(), "not enough raw space in edata section to repack export names");
}};

const Case zeroFileAlignment{"PatchNids_ZeroFileAlignment_Throws", [] {
    auto builder = ThreeExports();
    builder.FileAlignment = 0;
    RequirePatchFails(builder.Build(), "invalid file alignment");
}};

const Case ordinalOutOfRange{"PatchNids_OrdinalPastFunctionCount_Throws", [] {
    PeImageBuilder builder;
    builder.Exports = {{"sceKernelUsleep", 1}};
    RequirePatchFails(builder.Build(), "export name ordinal out of bounds");
}};

const Case duplicatePatchedName{"PatchNids_TwoNamesWithSameNid_Throws", [] {
    PeImageBuilder builder;
    builder.Exports = {{"printf_nid_disambig1", 0}, {"printf_nid_disambig2", 1}};
    RequirePatchFails(builder.Build(), "duplicate patched export name: " + printfNid);
}};

} // namespace
