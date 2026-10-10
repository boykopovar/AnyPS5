#include <Testing/Test.hpp>
#include <elfpatcher/windows/WindowsIconResourceBuilder.hpp>
#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <domain/Types.hpp>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace Windows = Elfpatcher::Windows;

const std::vector<std::uint8_t> Png{
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4,
    0x89, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
    0x1f, 0x00, 0x05, 0x00, 0x01, 0xff, 0x89, 0x99, 0x3d, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

constexpr std::uint32_t NextRva = 0x50000;
constexpr std::size_t ResourceHeaderSize = 184;

std::uint32_t readU32(const std::vector<std::uint8_t>& data, const std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) | static_cast<std::uint32_t>(data[offset + 1]) << 8 |
           static_cast<std::uint32_t>(data[offset + 2]) << 16 | static_cast<std::uint32_t>(data[offset + 3]) << 24;
}

void writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Testing::Require(static_cast<bool>(stream), "Cannot write " + path.string());
}

void requireEmpty(const std::filesystem::path& path, const std::string& name) {
    std::vector<Windows::PeSection> sections;

    const auto directory = Windows::WindowsIconResourceBuilder().Build(path, sections, 0x10000);

    Testing::Require(sections.empty(), "Section added for " + name);
    Testing::RequireEqual(directory.Rva, std::uint32_t{0}, "Resource directory RVA for " + name);
    Testing::RequireEqual(directory.Size, std::uint32_t{0}, "Resource directory size for " + name);
}

struct BuiltIcon {
    std::vector<Windows::PeSection> Sections;
    Windows::PeDirectory Directory;
};

BuiltIcon buildValidIcon(const std::filesystem::path& root) {
    const auto iconPath = root / "valid" / "icon0.png";
    writeBytes(iconPath, Png);
    BuiltIcon built;
    built.Directory = Windows::WindowsIconResourceBuilder().Build(iconPath, built.Sections, NextRva);
    Testing::RequireEqual(built.Sections.size(), std::size_t{1}, "Expected one section");
    return built;
}

std::filesystem::path writeMalformedIcon(const std::filesystem::path& root) {
    const auto badPath = root / "bad" / "icon0.png";
    auto truncated = Png;
    truncated.resize(40);
    writeBytes(badPath, truncated);
    return badPath;
}

const Testing::Case emptyPathAddsNothing{"WindowsIconResource_EmptyPath_AddsNoResource", [] {
    requireEmpty({}, "an empty path");
}};

const Testing::Case missingIconAddsNothing{"WindowsIconResource_MissingIcon_AddsNoResource", [] {
    const Testing::TemporaryDirectory root;

    requireEmpty(root.Path() / "missing" / "icon0.png", "a missing icon");
}};

const Testing::Case validIconAddsRsrcSection{"WindowsIconResource_ValidIcon_AddsReadableRsrcSection", [] {
    const Testing::TemporaryDirectory root;

    const auto built = buildValidIcon(root.Path());

    const auto& section = built.Sections[0];
    Testing::RequireEqual(section.Name, std::string(".rsrc"), "Unexpected section name");
    Testing::RequireEqual(section.Rva, NextRva, "Unexpected section RVA");
    Testing::RequireEqual(section.Characteristics, Windows::SectionRead | 0x40u, "Unexpected section characteristics");
    Testing::RequireEqual(section.Data.size(), ResourceHeaderSize + Png.size(), "Unexpected section size");
}};

const Testing::Case validIconSetsResourceDirectory{"WindowsIconResource_ValidIcon_SetsResourceDirectoryToSection", [] {
    const Testing::TemporaryDirectory root;

    const auto built = buildValidIcon(root.Path());

    Testing::RequireEqual(built.Directory.Rva, NextRva, "Unexpected resource directory RVA");
    Testing::RequireEqual(static_cast<std::size_t>(built.Directory.Size), built.Sections[0].Data.size(), "Unexpected resource directory size");
}};

const Testing::Case validIconDeclaresResourceTypes{"WindowsIconResource_ValidIcon_DeclaresIconAndGroupIconTypes", [] {
    const Testing::TemporaryDirectory root;

    const auto built = buildValidIcon(root.Path());

    const auto& data = built.Sections[0].Data;
    Testing::RequireEqual(readU32(data, 16), std::uint32_t{3}, "Unexpected RT_ICON type");
    Testing::RequireEqual(readU32(data, 24), std::uint32_t{14}, "Unexpected RT_GROUP_ICON type");
}};

const Testing::Case validIconWritesDataEntries{"WindowsIconResource_ValidIcon_WritesIconAndGroupDataEntries", [] {
    const Testing::TemporaryDirectory root;

    const auto built = buildValidIcon(root.Path());

    const auto& data = built.Sections[0].Data;
    Testing::RequireEqual(readU32(data, 128), static_cast<std::uint32_t>(NextRva + ResourceHeaderSize), "Unexpected RT_ICON data entry RVA");
    Testing::RequireEqual(readU32(data, 132), static_cast<std::uint32_t>(Png.size()), "Unexpected RT_ICON data entry size");
    Testing::RequireEqual(readU32(data, 144), NextRva + 160, "Unexpected RT_GROUP_ICON data entry RVA");
    Testing::RequireEqual(readU32(data, 148), std::uint32_t{20}, "Unexpected RT_GROUP_ICON data entry size");
}};

const Testing::Case validIconWritesGroupEntry{"WindowsIconResource_ValidIcon_WritesGroupEntryDimensionsAndSize", [] {
    const Testing::TemporaryDirectory root;

    const auto built = buildValidIcon(root.Path());

    const auto& data = built.Sections[0].Data;
    Testing::RequireEqual(data[166], std::uint8_t{1}, "Unexpected icon width");
    Testing::RequireEqual(data[167], std::uint8_t{1}, "Unexpected icon height");
    Testing::RequireEqual(readU32(data, 174), static_cast<std::uint32_t>(Png.size()), "Unexpected icon size in group entry");
}};

const Testing::Case validIconKeepsBytes{"WindowsIconResource_ValidIcon_EmbedsPngBytesUnchanged", [] {
    const Testing::TemporaryDirectory root;

    const auto built = buildValidIcon(root.Path());

    const auto& data = built.Sections[0].Data;
    Testing::Require(std::equal(Png.begin(), Png.end(), data.begin() + ResourceHeaderSize), "Icon bytes changed");
}};

const Testing::Case malformedIconNamesPath{"WindowsIconResource_MalformedIcon_ThrowsNamingIconPath", [] {
    const Testing::TemporaryDirectory root;
    const auto badPath = writeMalformedIcon(root.Path());
    std::vector<Windows::PeSection> sections;

    const auto error = Testing::RequireThrows<Domain::RelinkerException>(
        [&] { Windows::WindowsIconResourceBuilder().Build(badPath, sections, NextRva); }, "Build succeeded for a malformed icon");

    Testing::Require(std::string(error.what()).find(badPath.string()) != std::string::npos, std::string("Failure does not name the icon: ") + error.what());
}};

const Testing::Case malformedIconAddsNoSection{"WindowsIconResource_MalformedIcon_AddsNoSection", [] {
    const Testing::TemporaryDirectory root;
    const auto badPath = writeMalformedIcon(root.Path());
    std::vector<Windows::PeSection> sections;

    Testing::RequireThrows<Domain::RelinkerException>(
        [&] { Windows::WindowsIconResourceBuilder().Build(badPath, sections, NextRva); }, "Build succeeded for a malformed icon");

    Testing::Require(sections.empty(), "Section added despite the failure");
}};

} // namespace
