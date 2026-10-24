#include <elfpatcher/windows/WindowsIconResourceBuilder.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>
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
    const auto fail = [&path](const std::string& reason) {
        const auto utf8 = path.u8string();
        throw Domain::RelinkerException("Invalid Windows icon PNG '" + std::string(utf8.begin(), utf8.end()) + "': " + reason);
    };
    constexpr std::array<std::uint8_t, 8> signature{0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (bytes.size() < 33 || !std::equal(signature.begin(), signature.end(), bytes.begin())) fail("missing PNG signature or IHDR");
    if (readBigEndian(bytes, 8) != 13 || readBigEndian(bytes, 12) != 0x49484452) fail("invalid IHDR size or type");
    const auto width = readBigEndian(bytes, 16);
    const auto height = readBigEndian(bytes, 20);
    if (width == 0 || height == 0 || width > 0x7fffffffu || height > 0x7fffffffu) fail("invalid IHDR dimensions");
    const auto depth = bytes[24];
    const auto color = bytes[25];
    const bool validDepth = (color == 0 && (depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16)) ||
                            (color == 3 && (depth == 1 || depth == 2 || depth == 4 || depth == 8)) ||
                            ((color == 2 || color == 4 || color == 6) && (depth == 8 || depth == 16));
    if (!validDepth || bytes[26] != 0 || bytes[27] != 0 || bytes[28] > 1) fail("invalid IHDR encoding");
    bool hasData = false;
    bool dataStarted = false;
    bool dataEnded = false;
    bool hasPalette = false;
    for (std::size_t offset = 8; offset < bytes.size();) {
        if (bytes.size() - offset < 12) fail("truncated chunk");
        const auto length = readBigEndian(bytes, offset);
        if (length > bytes.size() - offset - 12) fail("truncated chunk data");
        const auto type = readBigEndian(bytes, offset + 4);
        for (std::size_t index = offset + 4; index < offset + 8; ++index)
            if (!((bytes[index] >= 'A' && bytes[index] <= 'Z') || (bytes[index] >= 'a' && bytes[index] <= 'z')))
                fail("invalid chunk type");
        std::uint32_t crc = 0xffffffffu;
        for (std::size_t index = offset + 4; index < offset + 8 + length; ++index) {
            crc ^= bytes[index];
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
        }
        if ((crc ^ 0xffffffffu) != readBigEndian(bytes, offset + 8 + length)) fail("invalid chunk CRC");
        if (type == 0x49484452 && offset != 8) fail("duplicate IHDR");
        if (type == 0x504c5445) {
            if (hasPalette || dataStarted || !length || length % 3 || length > 768 || color == 0 || color == 4 ||
                (color == 3 && length / 3 > (1u << depth))) fail("invalid PLTE");
            hasPalette = true;
        }
        if (type == 0x49444154) {
            if (dataEnded || (color == 3 && !hasPalette)) fail("invalid IDAT order or missing PLTE");
            dataStarted = true;
            if (length != 0) hasData = true;
        } else {
            if (dataStarted) dataEnded = true;
            if (!(type & 0x20000000u) && type != 0x49484452 && type != 0x504c5445 && type != 0x49454e44)
                fail("unknown critical chunk");
        }
        offset += length + 12;
        if (type == 0x49454e44) {
            if (length != 0 || offset != bytes.size() || !hasData) fail("invalid IEND or missing IDAT");
            return;
        }
    }
    fail("missing IEND");
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
