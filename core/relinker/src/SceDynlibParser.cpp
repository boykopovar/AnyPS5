#include <relinker/SceDynlibParser.hpp>
#include <relinker/ISdkRevisionProfile.hpp>
#include <cstring>
#include <sstream>

namespace Relinker {

static constexpr std::uint64_t NID_INDEX_MASK = 0x000000000000FFFFULL;
static constexpr std::uint64_t LIB_INDEX_MASK = 0x00000000FFFF0000ULL;
static constexpr int LIB_INDEX_SHIFT = 16;
static constexpr std::uint32_t RELOC_TYPE_MASK = 0xFFFFFFFFU;

SceDynlibParser::SceDynlibParser(std::shared_ptr<ISdkRevisionProfile> SdkProfile)
    : _sdkProfile(std::move(SdkProfile)) {
}

std::uint8_t SceDynlibParser::_readU8At(
    const std::vector<std::uint8_t>& Data, FileByteOffset FileByteOffset) const {
    if (FileByteOffset >= Data.size()) {
        throw RelinkerException("SCE dynlib data offset out of bounds", FileByteOffset);
    }
    return Data[FileByteOffset];
}

std::uint16_t SceDynlibParser::_readU16At(
    const std::vector<std::uint8_t>& Data, FileByteOffset FileByteOffset) const {
    if (FileByteOffset + 2 > Data.size()) {
        throw RelinkerException("SCE dynlib data offset out of bounds", FileByteOffset);
    }
    std::uint16_t value;
    std::memcpy(&value, Data.data() + FileByteOffset, 2);
    return value;
}

std::uint32_t SceDynlibParser::_readU32At(
    const std::vector<std::uint8_t>& Data, FileByteOffset FileByteOffset) const {
    if (FileByteOffset + 4 > Data.size()) {
        throw RelinkerException("SCE dynlib data offset out of bounds", FileByteOffset);
    }
    std::uint32_t value;
    std::memcpy(&value, Data.data() + FileByteOffset, 4);
    return value;
}

std::uint64_t SceDynlibParser::_readU64At(
    const std::vector<std::uint8_t>& Data, FileByteOffset FileByteOffset) const {
    if (FileByteOffset + 8 > Data.size()) {
        throw RelinkerException("SCE dynlib data offset out of bounds", FileByteOffset);
    }
    std::uint64_t value;
    std::memcpy(&value, Data.data() + FileByteOffset, 8);
    return value;
}

std::vector<LibraryImport> SceDynlibParser::ParseLibraryImports(
    const std::vector<std::uint8_t>& SceDynlibData) {
    std::uint32_t count = _sdkProfile->GetImportsCount(SceDynlibData);
    std::uint32_t baseOff = _sdkProfile->GetImportsOffset();
    std::uint32_t entrySize = _sdkProfile->GetSceDynlibDataEntrySize();

    std::vector<LibraryImport> imports;
    imports.reserve(count);

    FileByteOffset cursor = baseOff;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (cursor + entrySize > SceDynlibData.size()) {
            throw RelinkerException("Library import entry out of bounds", cursor);
        }

        std::uint32_t nameOffset = _readU32At(SceDynlibData, cursor);
        std::uint32_t libraryId = _readU16At(SceDynlibData, cursor + 0x04);
        std::uint32_t version = _readU16At(SceDynlibData, cursor + 0x06);

        std::string name;
        FileByteOffset namePos = nameOffset;
        while (namePos < SceDynlibData.size() && SceDynlibData[namePos] != '\0') {
            name += static_cast<char>(SceDynlibData[namePos]);
            ++namePos;
        }

        LibraryImport imp;
        imp.LibraryName = std::move(name);
        imp.LibraryId = libraryId;
        imp.Version = version;
        imports.push_back(std::move(imp));

        cursor += entrySize;
    }

    return imports;
}

std::vector<RelocationEntry> SceDynlibParser::ParseRelocationTable(
    const std::vector<std::uint8_t>& SceDynlibData) {
    std::uint32_t tableOffset = _sdkProfile->GetRelocationTableOffset();
    std::uint32_t entrySize = _sdkProfile->GetRelocationEntrySize();

    if (tableOffset >= SceDynlibData.size()) {
        return {};
    }

    std::vector<RelocationEntry> entries;
    FileByteOffset cursor = tableOffset;

    while (cursor + entrySize <= SceDynlibData.size()) {
        RelocationEntry entry;
        entry.EntryOffset = _readU64At(SceDynlibData, cursor);
        entry.Info = _readU64At(SceDynlibData, cursor + 0x08);
        entry.Addend = static_cast<std::int64_t>(_readU64At(SceDynlibData, cursor + 0x10));
        entries.push_back(entry);
        cursor += entrySize;
    }

    return entries;
}

std::string SceDynlibParser::_extractNidFromRelocationInfo(std::uint64_t Info) const {
    std::uint64_t symIndex = Info >> 32;
    std::ostringstream oss;
    oss << std::hex << symIndex;
    return oss.str();
}

std::string SceDynlibParser::_getLibraryNameForRelocation(
    const std::vector<LibraryImport>& Imports, std::uint64_t Info) const {
    std::uint32_t libIndex = static_cast<std::uint32_t>((Info & LIB_INDEX_MASK) >> LIB_INDEX_SHIFT);
    if (libIndex == 0 || libIndex > Imports.size()) {
        return "";
    }
    return Imports[libIndex - 1].LibraryName;
}

std::vector<NidReference> SceDynlibParser::ExtractNidReferences(
    const std::vector<RelocationEntry>& Relocations,
    const std::vector<LibraryImport>& Imports) {
    std::vector<NidReference> refs;
    refs.reserve(Relocations.size());

    for (std::size_t i = 0; i < Relocations.size(); ++i) {
        const auto& reloc = Relocations[i];
        std::uint32_t relocType = static_cast<std::uint32_t>(reloc.Info & RELOC_TYPE_MASK);

        std::string nid = _extractNidFromRelocationInfo(reloc.Info);
        std::string library = _getLibraryNameForRelocation(Imports, reloc.Info);

        NidReference ref;
        ref.Nid = std::move(nid);
        ref.Library = std::move(library);
        ref.RelocationTypeValue = relocType;
        ref.RelocationTableOffset = static_cast<FileByteOffset>(i * _sdkProfile->GetRelocationEntrySize());
        ref.RelocationAddress = reloc.EntryOffset;
        refs.push_back(std::move(ref));
    }

    return refs;
}

}
