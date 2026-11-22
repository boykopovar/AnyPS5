#include <elfpatcher/windows/WindowsIconResourceBuilder.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>
#include <io/PngValidation.hpp>
#include <algorithm>
#include <array>
#include <string_view>

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

constexpr std::string_view Manifest =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
    "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\r\n"
    "  <application xmlns=\"urn:schemas-microsoft-com:asm.v3\">\r\n"
    "    <windowsSettings>\r\n"
    "      <heapType xmlns=\"http://schemas.microsoft.com/SMI/2020/WindowsSettings\">SegmentHeap</heapType>\r\n"
    "    </windowsSettings>\r\n"
    "  </application>\r\n"
    "</assembly>\r\n";

}

PeDirectory WindowsIconResourceBuilder::Build(const std::optional<std::vector<std::uint8_t>>& png, const std::filesystem::path& iconPath, std::vector<PeSection>& sections, const std::uint32_t nextRva) const {
    if (nextRva < LoadRva || nextRva % SectionAlignment != 0)
        throw Domain::RelinkerException("Invalid Windows resource section RVA", nextRva);
    if (std::any_of(sections.begin(), sections.end(), [](const auto& section) { return section.Name == ".rsrc"; }))
        throw Domain::RelinkerException("Duplicate Windows resource section");
    if (!sections.empty() && nextRva < AlignRva(static_cast<std::uint64_t>(sections.back().Rva) + sections.back().MemorySize()))
        throw Domain::RelinkerException("Windows resource section overlaps preceding section", nextRva);
    std::vector<std::pair<std::uint16_t, std::vector<std::uint8_t>>> resources;
    if (png) {
        if (png->size() > 0x7fffffffu - 512) throw Domain::RelinkerException("Invalid Windows icon size");
        validatePng(*png, iconPath);
        std::vector<std::uint8_t> group(20);
        Io::WriteU16(group, 2, 1);
        Io::WriteU16(group, 4, 1);
        const auto width = readBigEndian(*png, 16);
        const auto height = readBigEndian(*png, 20);
        group[6] = width < 256 ? static_cast<std::uint8_t>(width) : 0;
        group[7] = height < 256 ? static_cast<std::uint8_t>(height) : 0;
        Io::WriteU16(group, 10, 1);
        Io::WriteU16(group, 12, 32);
        Io::WriteU32(group, 14, CheckedRva(png->size()));
        Io::WriteU16(group, 18, 1);
        resources.emplace_back(3, *png);
        resources.emplace_back(14, std::move(group));
    }
    resources.emplace_back(24, std::vector<std::uint8_t>(Manifest.begin(), Manifest.end()));
    const std::size_t count = resources.size();
    const std::size_t names = 16 + 8 * count;
    const std::size_t languages = names + 24 * count;
    const std::size_t entries = languages + 24 * count;
    std::size_t size = entries + 16 * count;
    std::vector<std::size_t> offsets;
    for (const auto& resource : resources) {
        size = (size + 7) & ~std::size_t{7};
        offsets.push_back(size);
        size += resource.second.size();
    }
    std::vector<std::uint8_t> data(size);
    CheckedRva(static_cast<std::uint64_t>(nextRva) + data.size());
    Io::WriteU16(data, 14, static_cast<std::uint16_t>(count));
    for (std::size_t index = 0; index < count; ++index) {
        const auto name = names + 24 * index;
        const auto language = languages + 24 * index;
        const auto entry = entries + 16 * index;
        Io::WriteU32(data, 16 + 8 * index, resources[index].first);
        Io::WriteU32(data, 20 + 8 * index, 0x80000000u | static_cast<std::uint32_t>(name));
        Io::WriteU16(data, name + 14, 1);
        Io::WriteU32(data, name + 16, 1);
        Io::WriteU32(data, name + 20, 0x80000000u | static_cast<std::uint32_t>(language));
        Io::WriteU16(data, language + 14, 1);
        Io::WriteU32(data, language + 20, static_cast<std::uint32_t>(entry));
        Io::WriteU32(data, entry, nextRva + static_cast<std::uint32_t>(offsets[index]));
        Io::WriteU32(data, entry + 4, CheckedRva(resources[index].second.size()));
        std::copy(resources[index].second.begin(), resources[index].second.end(), data.begin() + static_cast<std::ptrdiff_t>(offsets[index]));
    }
    const PeDirectory directory{nextRva, CheckedRva(data.size())};
    sections.push_back({".rsrc", nextRva, SectionRead | 0x40u, std::move(data)});
    return directory;
}
}
