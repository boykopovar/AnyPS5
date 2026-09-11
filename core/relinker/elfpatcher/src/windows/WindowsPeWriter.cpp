#include <elfpatcher/windows/WindowsPeWriter.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>

namespace Elfpatcher::Windows {

std::vector<std::uint8_t> WindowsPeWriter::Write(const std::vector<PeSection>& sections, const std::uint32_t entryRva, const std::array<PeDirectory, 16>& directories) const {
    constexpr std::size_t peOffset = 0x80;
    constexpr std::size_t optionalOffset = peOffset + 24;
    constexpr std::size_t sectionTable = optionalOffset + 240;
    if (sections.empty() || sections.size() > 96 || sectionTable + sections.size() * 40 > LoadRva)
        throw Domain::RelinkerException("Invalid PE section count");
    std::vector<std::uint8_t> result(LoadRva);
    Io::WriteU16(result, 0, 0x5a4d);
    Io::WriteU32(result, 0x3c, peOffset);
    Io::WriteU32(result, peOffset, 0x4550);
    Io::WriteU16(result, peOffset + 4, 0x8664);
    Io::WriteU16(result, peOffset + 6, static_cast<std::uint16_t>(sections.size()));
    Io::WriteU16(result, peOffset + 20, 240);
    Io::WriteU16(result, peOffset + 22, directories[5].Size == 0 ? 0x23 : 0x22);
    Io::WriteU16(result, optionalOffset, 0x20b);
    Io::WriteU32(result, optionalOffset + 16, entryRva);
    Io::WriteU64(result, optionalOffset + 24, ImageBase);
    Io::WriteU32(result, optionalOffset + 32, SectionAlignment);
    Io::WriteU32(result, optionalOffset + 36, FileAlignment);
    Io::WriteU16(result, optionalOffset + 40, 6);
    Io::WriteU16(result, optionalOffset + 42, 2);
    Io::WriteU16(result, optionalOffset + 48, 6);
    Io::WriteU16(result, optionalOffset + 50, 2);
    Io::WriteU32(result, optionalOffset + 60, LoadRva);
    Io::WriteU16(result, optionalOffset + 68, 3);
    Io::WriteU16(result, optionalOffset + 70, directories[5].Size == 0 ? 0x100 : 0x160);
    Io::WriteU64(result, optionalOffset + 72, 0x210000);
    Io::WriteU64(result, optionalOffset + 80, 0x200000);
    Io::WriteU64(result, optionalOffset + 88, 0x100000);
    Io::WriteU64(result, optionalOffset + 96, 0x1000);
    Io::WriteU32(result, optionalOffset + 108, static_cast<std::uint32_t>(directories.size()));
    std::uint32_t endRva = LoadRva;
    std::uint32_t codeSize = 0;
    std::uint32_t dataSize = 0;
    bool foundEntry = false;
    for (std::size_t index = 0; index < sections.size(); ++index) {
        const auto& section = sections[index];
        if (section.Name.empty() || section.Name.size() > 8 || section.Data.empty() || section.Rva != endRva)
            throw Domain::RelinkerException("Invalid PE section layout: " + section.Name, section.Rva);
        const auto rawSize = Io::AlignUp(CheckedRva(section.Data.size()), FileAlignment);
        const auto rawOffset = CheckedRva(result.size());
        const auto header = sectionTable + index * 40;
        std::copy(section.Name.begin(), section.Name.end(), result.begin() + static_cast<std::ptrdiff_t>(header));
        Io::WriteU32(result, header + 8, CheckedRva(section.Data.size()));
        Io::WriteU32(result, header + 12, section.Rva);
        Io::WriteU32(result, header + 16, rawSize);
        Io::WriteU32(result, header + 20, rawOffset);
        Io::WriteU32(result, header + 36, section.Characteristics);
        if ((section.Characteristics & SectionExecute) != 0) {
            if (codeSize == 0)
                Io::WriteU32(result, optionalOffset + 20, section.Rva);
            codeSize = CheckedRva(static_cast<std::uint64_t>(codeSize) + rawSize);
            foundEntry |= entryRva >= section.Rva && entryRva - section.Rva < section.Data.size();
        } else {
            dataSize = CheckedRva(static_cast<std::uint64_t>(dataSize) + rawSize);
        }
        result.insert(result.end(), section.Data.begin(), section.Data.end());
        result.resize(static_cast<std::size_t>(rawOffset) + rawSize);
        endRva = AlignRva(section.Rva + static_cast<std::uint64_t>(section.Data.size()));
    }
    if (!foundEntry)
        throw Domain::RelinkerException("PE entry point is not inside executable section data", entryRva);
    Io::WriteU32(result, optionalOffset + 4, codeSize);
    Io::WriteU32(result, optionalOffset + 8, dataSize);
    Io::WriteU32(result, optionalOffset + 56, endRva);
    for (std::size_t index = 0; index < directories.size(); ++index) {
        const auto& directory = directories[index];
        if ((directory.Rva == 0) != (directory.Size == 0) || directory.Rva > endRva || directory.Size > endRva - directory.Rva)
            throw Domain::RelinkerException("Invalid PE data directory", index);
        Io::WriteU32(result, optionalOffset + 112 + index * 8, directory.Rva);
        Io::WriteU32(result, optionalOffset + 116 + index * 8, directory.Size);
    }
    return result;
}

}
