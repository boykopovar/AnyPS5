#include <pkg/ContentContainer.hpp>
#include <pkg/ByteOrder.hpp>
#include <pkg/PackageError.hpp>
#include <algorithm>

namespace Pkg {

namespace {

constexpr std::uint32_t EntryNamesId = 0x200;
constexpr std::uint32_t EncryptedEntry = 0x80000000;
constexpr std::uint32_t MaximumEntries = 0x10000;

}

std::string ReadContentId(const PackageFile& file, const std::uint64_t containerOffset) {
    if (containerOffset > file.Size() || file.Size() - containerOffset < 0x64) throw PackageError("Invalid package content container offset", containerOffset);
    const auto bytes = file.Read(containerOffset + 0x40, 0x24);
    const auto end = std::find(bytes.begin(), bytes.end(), 0);
    return std::string(bytes.begin(), end);
}

std::vector<ContentEntry> ReadNamedContentEntries(const PackageFile& file, const std::uint64_t containerOffset) {
    if (containerOffset > file.Size() || file.Size() - containerOffset < 0x20) throw PackageError("Invalid package content container offset", containerOffset);
    const auto header = file.Read(containerOffset, 0x20);
    if (header[0] != 0x7f || header[1] != 'C' || header[2] != 'N' || header[3] != 'T') throw PackageError("Missing package content container", containerOffset);
    const std::uint32_t count = LoadBig32(header.data() + 0x10);
    const std::uint32_t tableOffset = LoadBig32(header.data() + 0x18);
    if (count > MaximumEntries || tableOffset > file.Size() - containerOffset || count * std::uint64_t{0x20} > file.Size() - containerOffset - tableOffset)
        throw PackageError("Invalid package content entry table", containerOffset);
    const auto table = file.Read(containerOffset + tableOffset, count * std::size_t{0x20});

    const auto entryRange = [&](const std::size_t index) {
        const std::uint8_t* entry = table.data() + index * 0x20;
        const std::uint64_t offset = LoadBig32(entry + 0x10);
        const std::uint32_t size = LoadBig32(entry + 0x14);
        if (offset > file.Size() - containerOffset || size > file.Size() - containerOffset - offset) throw PackageError("Package content entry lies outside the file");
        return std::pair{containerOffset + offset, size};
    };

    std::vector<std::uint8_t> names;
    for (std::size_t index = 0; index < count; ++index) {
        if (LoadBig32(table.data() + index * 0x20) != EntryNamesId) continue;
        const auto [offset, size] = entryRange(index);
        names = file.Read(offset, size);
    }

    std::vector<ContentEntry> entries;
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint8_t* entry = table.data() + index * 0x20;
        const std::uint32_t nameOffset = LoadBig32(entry + 4);
        if (nameOffset == 0 || LoadBig32(entry) == EntryNamesId) continue;
        if (nameOffset >= names.size()) throw PackageError("Package content entry name lies outside the name table");
        const auto end = std::find(names.begin() + nameOffset, names.end(), 0);
        if (end == names.end()) throw PackageError("Unterminated package content entry name");
        std::string name(names.begin() + nameOffset, end);
        if ((LoadBig32(entry + 8) & EncryptedEntry) != 0) throw PackageError("Encrypted package content entry is not supported: " + name);
        const auto [offset, size] = entryRange(index);
        entries.push_back(ContentEntry{std::move(name), offset, size});
    }
    return entries;
}

}
