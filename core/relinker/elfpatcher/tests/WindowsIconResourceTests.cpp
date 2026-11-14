#include <elfpatcher/windows/WindowsIconResourceBuilder.hpp>
#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <domain/Types.hpp>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
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

std::pair<std::uint32_t, std::uint32_t> singleEntry(const std::vector<std::uint8_t>& data, const std::size_t offset) {
    require(offset + 24 <= data.size() && readU32(data, offset + 12) == 0x10000u, "Resource directory does not hold one ID entry");
    return {readU32(data, offset + 16), readU32(data, offset + 20)};
}

std::map<std::uint32_t, std::vector<std::uint8_t>> readResources(const std::vector<std::uint8_t>& data, const std::uint32_t rva) {
    require(data.size() >= 16 && readU32(data, 12) >> 16 != 0 && (readU32(data, 12) & 0xffffu) == 0, "Resource root holds no ID entries");
    const std::size_t count = readU32(data, 12) >> 16;
    require(16 + count * 8 <= data.size(), "Resource root is truncated");
    std::map<std::uint32_t, std::vector<std::uint8_t>> result;
    std::uint32_t previous = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const auto type = readU32(data, 16 + index * 8);
        const auto target = readU32(data, 20 + index * 8);
        require(type > previous && (target & 0x80000000u) != 0, "Resource types are not sorted subdirectories");
        previous = type;
        const auto [name, languages] = singleEntry(data, target & 0x7fffffffu);
        require(name == 1 && (languages & 0x80000000u) != 0, "Unexpected resource name entry");
        const auto [language, leaf] = singleEntry(data, languages & 0x7fffffffu);
        require(language == 0 && (leaf & 0x80000000u) == 0 && leaf % 4 == 0 && leaf + 16 <= data.size(), "Unexpected resource language entry");
        const auto offset = readU32(data, leaf) - rva;
        const auto size = readU32(data, leaf + 4);
        require(offset % 4 == 0 && offset <= data.size() && size <= data.size() - offset, "Resource data is outside the section");
        result[type] = std::vector<std::uint8_t>(data.begin() + offset, data.begin() + offset + size);
    }
    return result;
}

std::vector<std::uint8_t> chunk(const std::string& type, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> result;
    const auto append = [&result](std::uint32_t value) {
        for (const auto shift : {24, 16, 8, 0}) result.push_back(static_cast<std::uint8_t>(value >> shift));
    };
    append(static_cast<std::uint32_t>(payload.size()));
    result.insert(result.end(), type.begin(), type.end());
    result.insert(result.end(), payload.begin(), payload.end());
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t index = 4; index < result.size(); ++index) {
        crc ^= result[index];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
    append(crc ^ 0xffffffffu);
    return result;
}

void checkChunks() {
    const Elfpatcher::Windows::WindowsIconResourceBuilder builder;
    const auto check = [&](const std::string& name, const std::vector<std::vector<std::uint8_t>>& chunks, bool valid, const std::vector<std::uint8_t>& header = {}) {
        std::vector<std::uint8_t> image(Png.begin(), Png.begin() + (header.empty() ? 33 : 8));
        if (!header.empty()) {
            const auto ihdr = chunk("IHDR", header);
            image.insert(image.end(), ihdr.begin(), ihdr.end());
        }
        for (const auto& value : chunks) image.insert(image.end(), value.begin(), value.end());
        image.insert(image.end(), Png.end() - 12, Png.end());
        const std::filesystem::path path = std::filesystem::path(std::u8string(name.begin(), name.end())) / "icon.png";
        const auto utf8 = path.u8string();
        std::vector<Elfpatcher::Windows::PeSection> sections;
        bool rejected = false;
        try { builder.Build(image, path, sections, 0x10000); }
        catch (const Domain::RelinkerException& error) {
            rejected = true;
            require(std::string(error.what()).find(std::string(utf8.begin(), utf8.end())) != std::string::npos, "Icon failure omitted the path");
        }
        require(rejected != valid, "Unexpected validation result for " + name);
        require(sections.size() == (valid ? 1u : 0u), "Unexpected section mutation for " + name);
    };
    const std::vector<std::uint8_t> data(Png.begin() + 41, Png.begin() + 54);
    const auto idat = chunk("IDAT", data);
    const auto note = chunk("tEXt", {'n', 0, 'v'});
    check("unknown-critical", {chunk("ABCD", {}), idat}, false);
    check("ancillary", {note, idat}, true);
    check("private-ancillary", {chunk("vpAg", {}), idat}, true);
    check("reserved-ancillary", {chunk("tExT", {}), idat}, true);
    for (const auto& type : {"t1Xt", "tEX!", "tEX\x80"})
        check("invalid-chunk-type", {chunk(type, {}), idat}, false);
    const auto first = chunk("IDAT", {data.begin(), data.begin() + 6});
    const auto last = chunk("IDAT", {data.begin() + 6, data.end()});
    check("consecutive", {first, last}, true);
    check("nonconsecutive", {first, note, last}, false);
    const auto palette = chunk("PLTE", {255, 0, 0});
    check("late-palette", {idat, palette}, false);
    check("duplicate-palette", {palette, palette, idat}, false);
    check("palette", {palette, idat}, true);
    check("empty-palette", {chunk("PLTE", {}), idat}, false);
    check("partial-palette", {chunk("PLTE", {0, 1, 2, 3}), idat}, false);
    check("large-palette", {chunk("PLTE", std::vector<std::uint8_t>(771)), idat}, false);
    std::vector<std::uint8_t> header(Png.begin() + 16, Png.begin() + 29);
    header[8] = 1;
    header[9] = 3;
    const auto indexed = chunk("IDAT", {0x78, 0x9c, 0x63, 0x60, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01});
    check("indexed", {palette, indexed}, true, header);
    check("indexed-missing-palette", {indexed}, false, header);
    check("indexed-large-palette", {chunk("PLTE", std::vector<std::uint8_t>(9)), indexed}, false, header);
    header[9] = 0;
    check("grayscale-palette", {palette, indexed}, false, header);
}

void run() {
    const Elfpatcher::Windows::WindowsIconResourceBuilder builder;

    checkChunks();

    const std::filesystem::path iconPath = "valid/icon0.png";
    const std::uint32_t nextRva = 0x50000;
    std::vector<Elfpatcher::Windows::PeSection> sections;
    const auto directory = builder.Build(Png, iconPath, sections, nextRva);
    require(sections.size() == 1, "Expected one section");
    require(sections[0].Name == ".rsrc", "Unexpected section name: " + sections[0].Name);
    require(sections[0].Rva == nextRva, "Unexpected section RVA");
    require(sections[0].Characteristics == (Elfpatcher::Windows::SectionRead | 0x40u), "Unexpected section characteristics");
    const auto& data = sections[0].Data;
    require(directory.Rva == nextRva && directory.Size == data.size(), "Unexpected resource directory");
    const auto resources = readResources(data, nextRva);
    require(resources.size() == 3 && resources.contains(3) && resources.contains(14) && resources.contains(24), "Unexpected resource types");
    require(resources.at(3) == Png, "Icon bytes changed");
    const auto& group = resources.at(14);
    require(group.size() == 20 && group[6] == 1 && group[7] == 1 && readU32(group, 14) == Png.size(), "Unexpected RT_GROUP_ICON entry");
    const std::string manifest(resources.at(24).begin(), resources.at(24).end());
    require(manifest.find("<heapType xmlns=\"http://schemas.microsoft.com/SMI/2020/WindowsSettings\">SegmentHeap</heapType>") != std::string::npos, "Manifest does not select the segment heap");

    std::vector<Elfpatcher::Windows::PeSection> manifestOnly;
    const auto manifestDirectory = builder.Build(std::nullopt, {}, manifestOnly, nextRva);
    require(manifestOnly.size() == 1 && manifestDirectory.Rva == nextRva && manifestDirectory.Size == manifestOnly[0].Data.size(), "Expected a manifest-only resource section");
    const auto manifestResources = readResources(manifestOnly[0].Data, nextRva);
    require(manifestResources.size() == 1 && manifestResources.contains(24) && manifestResources.at(24) == resources.at(24), "Unexpected manifest-only resources");

    const std::filesystem::path badPath(u8"\u65e5\u672c/bad/icon0.png");
    const auto badUtf8 = badPath.u8string();
    auto truncated = Png;
    truncated.resize(40);
    std::vector<Elfpatcher::Windows::PeSection> badSections;
    bool caughtTruncated = false;
    try {
        builder.Build(truncated, badPath, badSections, nextRva);
    } catch (const Domain::RelinkerException& e) {
        require(std::string(e.what()).find(std::string(badUtf8.begin(), badUtf8.end())) != std::string::npos, std::string("Failure does not name the icon: ") + e.what());
        require(badSections.empty(), "Section added despite the failure");
        caughtTruncated = true;
    }
    require(caughtTruncated, "Build succeeded for a malformed icon");

    for (const auto badRva : {0u, 0x8000u, 0x10001u, 0x10800u}) {
        std::vector<Elfpatcher::Windows::PeSection> invalidRvaSections;
        bool caught = false;
        try {
            builder.Build(Png, iconPath, invalidRvaSections, badRva);
        } catch (const Domain::RelinkerException& error) {
            caught = true;
            require(std::string(error.what()).find("Invalid Windows resource section RVA") != std::string::npos, "Unexpected error for bad RVA");
            require(invalidRvaSections.empty(), "Section added on bad RVA");
        }
        require(caught, "Build succeeded for invalid resource section RVA");
    }

    std::vector<Elfpatcher::Windows::PeSection> duplicateSections{
        {".rsrc", 0x10000, Elfpatcher::Windows::SectionRead | 0x40u, std::vector<std::uint8_t>(0x1000, 0)}
    };
    bool caughtDuplicate = false;
    try {
        builder.Build(Png, iconPath, duplicateSections, 0x20000);
    } catch (const Domain::RelinkerException& error) {
        caughtDuplicate = true;
        require(std::string(error.what()).find("Duplicate Windows resource section") != std::string::npos, "Unexpected error for duplicate resource section");
        require(duplicateSections.size() == 1, "Section added on duplicate resource section");
    }
    require(caughtDuplicate, "Build succeeded with duplicate resource section");

    std::vector<Elfpatcher::Windows::PeSection> overlappingSections{
        {".text", 0x10000, Elfpatcher::Windows::SectionRead | 0x20u, std::vector<std::uint8_t>(0x50000, 0)}
    };
    bool caughtOverlap = false;
    try {
        builder.Build(Png, iconPath, overlappingSections, 0x50000);
    } catch (const Domain::RelinkerException& error) {
        caughtOverlap = true;
        require(std::string(error.what()).find("Windows resource section overlaps preceding section") != std::string::npos, "Unexpected error for overlapping section");
        require(overlappingSections.size() == 1, "Section added on overlapping section");
    }
    require(caughtOverlap, "Build succeeded with overlapping resource section");

    std::vector<Elfpatcher::Windows::PeSection> virtualOverlap{
        {".data", 0x10000, Elfpatcher::Windows::SectionRead | 0x40u, std::vector<std::uint8_t>(0x1000, 0), 0x41000}
    };
    bool caughtVirtualOverlap = false;
    try {
        builder.Build(Png, iconPath, virtualOverlap, 0x50000);
    } catch (const Domain::RelinkerException& error) {
        caughtVirtualOverlap = true;
        require(std::string(error.what()).find("Windows resource section overlaps preceding section") != std::string::npos, "Unexpected error for virtual overlap");
        require(virtualOverlap.size() == 1, "Section added on virtual overlap");
    }
    require(caughtVirtualOverlap, "Build succeeded with a resource section overlapping virtual data");
    const auto following = builder.Build(Png, iconPath, virtualOverlap, 0x60000);
    require(following.Rva == 0x60000 && virtualOverlap.size() == 2, "Build rejected a resource section after virtual data");
}

}

int main() {
    try {
        run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
