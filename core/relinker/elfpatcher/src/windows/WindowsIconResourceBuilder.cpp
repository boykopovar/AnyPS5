#include <elfpatcher/windows/WindowsIconResourceBuilder.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>
#include <io/PngValidation.hpp>
#include <algorithm>
#include <array>

#include <utility>

namespace Elfpatcher::Windows {

namespace {

std::uint32_t readBigEndian(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) << 24 | static_cast<std::uint32_t>(bytes[offset + 1]) << 16 |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 8 | bytes[offset + 3];
}

void validatePng(const std::vector<std::uint8_t>& bytes, const std::filesystem::path& path) {
    try {
        Io::Png::Validate(bytes);
    } catch (const std::invalid_argument& error) {
        const auto utf8 = path.u8string();
        throw Domain::RelinkerException("Invalid Windows icon PNG '" + std::string(utf8.begin(), utf8.end()) + "': " + error.what());
    }
}

}

PeDirectory WindowsIconResourceBuilder::Build(const std::vector<std::uint8_t>& png, const std::filesystem::path& iconPath, std::vector<PeSection>& sections, const std::uint32_t nextRva) const {
    if (nextRva < LoadRva || nextRva % SectionAlignment != 0)
        throw Domain::RelinkerException("Invalid Windows resource section RVA", nextRva);
    if (std::any_of(sections.begin(), sections.end(), [](const auto& section) { return section.Name == ".rsrc"; }))
        throw Domain::RelinkerException("Duplicate Windows resource section");
    if (!sections.empty() && nextRva < AlignRva(static_cast<std::uint64_t>(sections.back().Rva) + sections.back().Data.size()))
        throw Domain::RelinkerException("Windows resource section overlaps preceding section", nextRva);
    if (png.size() > 0x7fffffffu - 184) throw Domain::RelinkerException("Invalid Windows icon size");
    validatePng(png, iconPath);
    std::vector<std::uint8_t> data(184 + png.size());
    CheckedRva(static_cast<std::uint64_t>(nextRva) + data.size());
    Io::WriteU16(data, 14, 2);
    Io::WriteU32(data, 16, 3);
    Io::WriteU32(data, 20, 0x80000020u);
    Io::WriteU32(data, 24, 14);
    Io::WriteU32(data, 28, 0x80000038u);
    for (const auto offset : {32u, 56u, 80u, 104u}) Io::WriteU16(data, offset + 14, 1);
    Io::WriteU32(data, 48, 1);
    Io::WriteU32(data, 52, 0x80000050u);
    Io::WriteU32(data, 72, 1);
    Io::WriteU32(data, 76, 0x80000068u);
    Io::WriteU32(data, 100, 128);
    Io::WriteU32(data, 124, 144);
    Io::WriteU32(data, 128, nextRva + 184);
    Io::WriteU32(data, 132, CheckedRva(png.size()));
    Io::WriteU32(data, 144, nextRva + 160);
    Io::WriteU32(data, 148, 20);
    Io::WriteU16(data, 162, 1);
    Io::WriteU16(data, 164, 1);
    const auto width = readBigEndian(png, 16);
    const auto height = readBigEndian(png, 20);
    data[166] = width < 256 ? static_cast<std::uint8_t>(width) : 0;
    data[167] = height < 256 ? static_cast<std::uint8_t>(height) : 0;
    Io::WriteU16(data, 170, 1);
    Io::WriteU16(data, 172, 32);
    Io::WriteU32(data, 174, CheckedRva(png.size()));
    Io::WriteU16(data, 178, 1);
    std::copy(png.begin(), png.end(), data.begin() + 184);
    const PeDirectory directory{nextRva, CheckedRva(data.size())};
    sections.push_back({".rsrc", nextRva, SectionRead | 0x40u, std::move(data)});
    return directory;
}

}
