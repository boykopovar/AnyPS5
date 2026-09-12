#include <elfpatcher/windows/WindowsImportBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <set>

namespace Elfpatcher::Windows {

WindowsImports WindowsImportBuilder::Build(const std::uint32_t sectionRva) const {
    const std::vector<std::string> names = {"ExitProcess", "FormatMessageA", "GetFileAttributesA", "GetLastError", "GetModuleFileNameA", "GetModuleHandleA", "GetProcAddress", "GetStdHandle", "GetSystemDirectoryA", "LoadLibraryExA", "RaiseException", "VirtualAlloc", "WriteFile", "lstrcatA", "lstrcmpA", "lstrcmpiA", "lstrcpyA", "lstrlenA"};
    WindowsImports result{{".idata", sectionRva, SectionRead | SectionWrite | 0x40u, std::vector<std::uint8_t>(40)}, {sectionRva, 40}, {}, {}};
    auto& bytes = result.Section.Data;
    const auto lookupOffset = bytes.size();
    bytes.resize(bytes.size() + (names.size() + 1) * 8);
    const auto addressOffset = bytes.size();
    bytes.resize(bytes.size() + (names.size() + 1) * 8);
    result.AddressTable = {CheckedRva(sectionRva + addressOffset), CheckedRva((names.size() + 1) * 8)};
    for (std::size_t index = 0; index < names.size(); ++index) {
        const auto nameRva = CheckedRva(sectionRva + bytes.size());
        Io::AppendU16(bytes, 0);
        Io::AppendString(bytes, names[index]);
        Io::AlignBuffer(bytes, 2);
        Io::WriteU64(bytes, lookupOffset + index * 8, nameRva);
        Io::WriteU64(bytes, addressOffset + index * 8, nameRva);
        result.Functions.emplace(names[index], CheckedRva(sectionRva + addressOffset + index * 8));
    }
    const auto libraryRva = CheckedRva(sectionRva + bytes.size());
    Io::AppendString(bytes, "KERNEL32.dll");
    Io::WriteU32(bytes, 0, CheckedRva(sectionRva + lookupOffset));
    Io::WriteU32(bytes, 12, libraryRva);
    Io::WriteU32(bytes, 16, CheckedRva(sectionRva + addressOffset));
    return result;
}

std::vector<std::string> WindowsImportBuilder::ReadLibraries(const Domain::SysVDynamicSection& dynamicSection) const {
    const auto& bytes = dynamicSection.DynamicSegmentData;
    if (bytes.size() % 16 != 0)
        throw Domain::RelinkerException("Invalid dynamic segment size");
    std::vector<std::string> result;
    std::set<std::string> unique;
    for (std::size_t offset = 0; offset < bytes.size(); offset += 16) {
        const auto tag = Io::ReadU64(bytes, offset);
        if (tag != 1)
            throw Domain::RelinkerException("Unexpected tag in rebuilt ELF dependency table", offset);
        const auto nameOffset = Io::ReadU64(bytes, offset + 8);
        if (nameOffset >= dynamicSection.DynStrData.size())
            throw Domain::RelinkerException("DT_NEEDED string offset is out of bounds", nameOffset);
        auto name = ReadString(dynamicSection.DynStrData, static_cast<std::size_t>(nameOffset));
        if (name.empty() || name.find_first_of("/\\:") != std::string::npos || !unique.insert(name).second)
            throw Domain::RelinkerException("Invalid or duplicate DT_NEEDED library: " + name);
        result.push_back(std::move(name));
    }
    return result;
}

}
