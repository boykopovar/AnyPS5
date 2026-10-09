#include <elfpatcher/windows/WindowsIconResourceBuilder.hpp>
#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <domain/Types.hpp>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const std::vector<std::uint8_t> Png{
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4,
    0x89, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
    0x1f, 0x00, 0x05, 0x00, 0x01, 0xff, 0x89, 0x99, 0x3d, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint32_t readU32(const std::vector<std::uint8_t>& data, const std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) | static_cast<std::uint32_t>(data[offset + 1]) << 8 |
           static_cast<std::uint32_t>(data[offset + 2]) << 16 | static_cast<std::uint32_t>(data[offset + 3]) << 24;
}

void writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(stream), "Cannot write " + path.string());
}

void requireEmpty(const Elfpatcher::Windows::WindowsIconResourceBuilder& builder, const std::filesystem::path& path, const std::string& name) {
    std::vector<Elfpatcher::Windows::PeSection> sections;
    const auto directory = builder.Build(path, sections, 0x10000);
    require(sections.empty(), "Section added for " + name);
    require(directory.Rva == 0 && directory.Size == 0, "Resource directory set for " + name);
}

void run(const std::filesystem::path& root) {
    const Elfpatcher::Windows::WindowsIconResourceBuilder builder;

    requireEmpty(builder, {}, "an empty path");
    requireEmpty(builder, root / "missing" / "icon0.png", "a missing icon");

    const auto iconPath = root / "valid" / "icon0.png";
    writeBytes(iconPath, Png);
    const std::uint32_t nextRva = 0x50000;
    std::vector<Elfpatcher::Windows::PeSection> sections;
    const auto directory = builder.Build(iconPath, sections, nextRva);
    require(sections.size() == 1, "Expected one section");
    require(sections[0].Name == ".rsrc", "Unexpected section name: " + sections[0].Name);
    require(sections[0].Rva == nextRva, "Unexpected section RVA");
    require(sections[0].Characteristics == (Elfpatcher::Windows::SectionRead | 0x40u), "Unexpected section characteristics");
    const auto& data = sections[0].Data;
    require(data.size() == 184 + Png.size(), "Unexpected section size");
    require(directory.Rva == nextRva && directory.Size == data.size(), "Unexpected resource directory");
    require(readU32(data, 16) == 3 && readU32(data, 24) == 14, "Unexpected resource types");
    require(readU32(data, 128) == nextRva + 184 && readU32(data, 132) == Png.size(), "Unexpected RT_ICON data entry");
    require(readU32(data, 144) == nextRva + 160 && readU32(data, 148) == 20, "Unexpected RT_GROUP_ICON data entry");
    require(data[166] == 1 && data[167] == 1, "Unexpected icon dimensions");
    require(readU32(data, 174) == Png.size(), "Unexpected icon size in group entry");
    require(std::equal(Png.begin(), Png.end(), data.begin() + 184), "Icon bytes changed");

    const auto badPath = root / "bad" / "icon0.png";
    auto truncated = Png;
    truncated.resize(40);
    writeBytes(badPath, truncated);
    std::vector<Elfpatcher::Windows::PeSection> badSections;
    try {
        builder.Build(badPath, badSections, nextRva);
    } catch (const Domain::RelinkerException& e) {
        require(std::string(e.what()).find(badPath.string()) != std::string::npos, std::string("Failure does not name the icon: ") + e.what());
        require(badSections.empty(), "Section added despite the failure");
        return;
    }
    throw std::runtime_error("Build succeeded for a malformed icon");
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-icon-resource-tests-" + std::to_string(std::random_device{}()));
    std::filesystem::remove_all(root);
    try {
        run(root);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        std::filesystem::remove_all(root);
        return 1;
    }
    std::filesystem::remove_all(root);
    return 0;
}
