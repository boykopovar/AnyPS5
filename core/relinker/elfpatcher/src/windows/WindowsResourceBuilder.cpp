#include <elfpatcher/windows/WindowsResourceBuilder.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>

namespace Elfpatcher::Windows {

namespace {

constexpr std::uint32_t ResourceTypeIcon = 3;
constexpr std::uint32_t ResourceTypeGroupIcon = 14;
constexpr std::uint32_t ResourceLanguage = 0x0409;
constexpr std::uint32_t SubdirectoryFlag = 0x80000000;
constexpr std::size_t DirectoryHeaderSize = 16;
constexpr std::size_t DirectoryEntrySize = 8;
constexpr std::size_t DataEntrySize = 16;
constexpr std::size_t IconDirectorySize = 6;
constexpr std::size_t IconEntrySize = 16;
constexpr std::size_t GroupEntrySize = 14;
constexpr std::size_t PayloadAlignment = 4;

struct IconImage {
    std::uint8_t Width;
    std::uint8_t Height;
    std::uint8_t ColorCount;
    std::uint16_t Planes;
    std::uint16_t BitCount;
    std::uint32_t Size;
    std::uint32_t Offset;
};

std::vector<IconImage> readIcon(const std::vector<std::uint8_t>& icon) {
    if (icon.size() < IconDirectorySize)
        throw Domain::RelinkerException("Icon file is smaller than an ICONDIR header");
    if (Io::ReadU16(icon, 0) != 0 || Io::ReadU16(icon, 2) != 1)
        throw Domain::RelinkerException("Icon file is not an ICO image");
    const std::size_t count = Io::ReadU16(icon, 4);
    if (count == 0)
        throw Domain::RelinkerException("Icon file contains no images");
    if (icon.size() < IconDirectorySize + count * IconEntrySize)
        throw Domain::RelinkerException("Icon file is truncated before its directory entries");
    std::vector<IconImage> images;
    for (std::size_t index = 0; index < count; ++index) {
        const auto entry = IconDirectorySize + index * IconEntrySize;
        IconImage image{};
        image.Width = icon[entry];
        image.Height = icon[entry + 1];
        image.ColorCount = icon[entry + 2];
        image.Planes = Io::ReadU16(icon, entry + 4);
        image.BitCount = Io::ReadU16(icon, entry + 6);
        image.Size = Io::ReadU32(icon, entry + 8);
        image.Offset = Io::ReadU32(icon, entry + 12);
        if (image.Size == 0)
            throw Domain::RelinkerException("Icon image is empty", index);
        if (image.Offset > icon.size() || image.Size > icon.size() - image.Offset)
            throw Domain::RelinkerException("Icon image is outside the icon file", index);
        images.push_back(image);
    }
    return images;
}

void writeDirectory(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint16_t idEntries) {
    Io::WriteU16(bytes, offset + 14, idEntries);
}

void writeEntry(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint32_t id, const std::uint32_t target) {
    Io::WriteU32(bytes, offset, id);
    Io::WriteU32(bytes, offset + 4, target);
}

}

WindowsResources WindowsResourceBuilder::Build(const std::vector<std::uint8_t>& icon, const std::uint32_t sectionRva) const {
    const auto images = readIcon(icon);
    const auto count = images.size();
    const auto rootSize = DirectoryHeaderSize + 2 * DirectoryEntrySize;
    const auto iconTypeOffset = rootSize;
    const auto groupTypeOffset = iconTypeOffset + DirectoryHeaderSize + count * DirectoryEntrySize;
    const auto languageOffset = groupTypeOffset + DirectoryHeaderSize + DirectoryEntrySize;
    const auto languageSize = DirectoryHeaderSize + DirectoryEntrySize;
    const auto dataEntryOffset = languageOffset + (count + 1) * languageSize;
    const auto payloadOffset = Io::AlignUp(dataEntryOffset + (count + 1) * DataEntrySize, PayloadAlignment);
    const auto groupSize = IconDirectorySize + count * GroupEntrySize;

    std::vector<std::uint8_t> bytes(payloadOffset);
    writeDirectory(bytes, 0, 2);
    writeEntry(bytes, DirectoryHeaderSize, ResourceTypeIcon, CheckedRva(iconTypeOffset) | SubdirectoryFlag);
    writeEntry(bytes, DirectoryHeaderSize + DirectoryEntrySize, ResourceTypeGroupIcon, CheckedRva(groupTypeOffset) | SubdirectoryFlag);
    writeDirectory(bytes, iconTypeOffset, static_cast<std::uint16_t>(count));
    writeDirectory(bytes, groupTypeOffset, 1);
    for (std::size_t index = 0; index <= count; ++index) {
        const auto language = languageOffset + index * languageSize;
        const auto parent = index < count
            ? iconTypeOffset + DirectoryHeaderSize + index * DirectoryEntrySize
            : groupTypeOffset + DirectoryHeaderSize;
        writeEntry(bytes, parent, static_cast<std::uint32_t>(index < count ? index + 1 : 1), CheckedRva(language) | SubdirectoryFlag);
        writeDirectory(bytes, language, 1);
        writeEntry(bytes, language + DirectoryHeaderSize, ResourceLanguage, CheckedRva(dataEntryOffset + index * DataEntrySize));
    }

    std::vector<std::uint8_t> group(groupSize);
    Io::WriteU16(group, 2, 1);
    Io::WriteU16(group, 4, static_cast<std::uint16_t>(count));
    for (std::size_t index = 0; index < count; ++index) {
        const auto& image = images[index];
        const auto entry = IconDirectorySize + index * GroupEntrySize;
        group[entry] = image.Width;
        group[entry + 1] = image.Height;
        group[entry + 2] = image.ColorCount;
        Io::WriteU16(group, entry + 4, image.Planes);
        Io::WriteU16(group, entry + 6, image.BitCount);
        Io::WriteU32(group, entry + 8, image.Size);
        Io::WriteU16(group, entry + 12, static_cast<std::uint16_t>(index + 1));
    }

    for (std::size_t index = 0; index <= count; ++index) {
        const auto entry = dataEntryOffset + index * DataEntrySize;
        Io::WriteU32(bytes, entry, CheckedRva(sectionRva + bytes.size()));
        if (index < count) {
            const auto& image = images[index];
            const auto begin = icon.begin() + static_cast<std::ptrdiff_t>(image.Offset);
            Io::WriteU32(bytes, entry + 4, image.Size);
            bytes.insert(bytes.end(), begin, begin + static_cast<std::ptrdiff_t>(image.Size));
        } else {
            Io::WriteU32(bytes, entry + 4, CheckedRva(groupSize));
            bytes.insert(bytes.end(), group.begin(), group.end());
        }
        Io::AlignBuffer(bytes, PayloadAlignment);
    }

    const auto size = CheckedRva(bytes.size());
    return {{".rsrc", sectionRva, SectionRead | 0x40u, std::move(bytes)}, {sectionRva, size}};
}

}
