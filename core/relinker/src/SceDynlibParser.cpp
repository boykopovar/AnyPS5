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
    const std::vector<std::uint8_t>& data, const FileByteOffset fileByteOffset) const {
    if (fileByteOffset >= data.size()) {
        throw RelinkerException("SCE dynlib data offset out of bounds", fileByteOffset);
    }
    return data[fileByteOffset];
}

std::uint16_t SceDynlibParser::_readU16At(
    const std::vector<std::uint8_t>& data, const FileByteOffset fileByteOffset) const {
    if (fileByteOffset + 2 > data.size()) {
        throw RelinkerException("SCE dynlib data offset out of bounds", fileByteOffset);
    }
    std::uint16_t value;
    std::memcpy(&value, data.data() + fileByteOffset, 2);
    return value;
}

std::uint32_t SceDynlibParser::_readU32At(
    const std::vector<std::uint8_t>& data, const FileByteOffset fileByteOffset) const {
    if (fileByteOffset + 4 > data.size()) {
        throw RelinkerException("SCE dynlib data offset out of bounds", fileByteOffset);
    }
    std::uint32_t value;
    std::memcpy(&value, data.data() + fileByteOffset, 4);
    return value;
}

std::uint64_t SceDynlibParser::_readU64At(
    const std::vector<std::uint8_t>& data, const FileByteOffset fileByteOffset) const {
    if (fileByteOffset + 8 > data.size()) {
        throw RelinkerException("SCE dynlib data offset out of bounds", fileByteOffset);
    }
    std::uint64_t value;
    std::memcpy(&value, data.data() + fileByteOffset, 8);
    return value;
}

std::vector<LibraryImport> SceDynlibParser::ParseLibraryImports(
    const std::vector<std::uint8_t>& sceDynlibData) {
    std::uint32_t count = _sdkProfile->GetImportsCount(sceDynlibData);
    std::uint32_t baseOff = _sdkProfile->GetImportsOffset();
    std::uint32_t entrySize = _sdkProfile->GetSceDynlibDataEntrySize();

    std::vector<LibraryImport> imports;
    imports.reserve(count);

    FileByteOffset cursor = baseOff;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (cursor + entrySize > sceDynlibData.size()) {
            throw RelinkerException("Library import entry out of bounds", cursor);
        }

        std::uint32_t nameOffset = _readU32At(sceDynlibData, cursor);
        std::uint32_t libraryId = _readU16At(sceDynlibData, cursor + 0x04);
        std::uint32_t version = _readU16At(sceDynlibData, cursor + 0x06);

        std::string name;
        FileByteOffset namePos = nameOffset;
        while (namePos < sceDynlibData.size() && sceDynlibData[namePos] != '\0') {
            name += static_cast<char>(sceDynlibData[namePos]);
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
    const std::vector<std::uint8_t>& sceDynlibData) {
    const std::uint32_t tableOffset = _sdkProfile->GetRelocationTableOffset();
    const std::uint32_t entrySize = _sdkProfile->GetRelocationEntrySize();

    if (tableOffset >= sceDynlibData.size()) {
        return {};
    }

    std::vector<RelocationEntry> entries;
    FileByteOffset cursor = tableOffset;

    while (cursor + entrySize <= sceDynlibData.size()) {
        RelocationEntry entry;
        entry.EntryOffset = _readU64At(sceDynlibData, cursor);
        entry.Info = _readU64At(sceDynlibData, cursor + 0x08);
        entry.Addend = static_cast<std::int64_t>(_readU64At(sceDynlibData, cursor + 0x10));
        entries.push_back(entry);
        cursor += entrySize;
    }

    return entries;
}

std::string SceDynlibParser::_extractNidFromRelocationInfo(std::uint64_t info) const {
    std::uint64_t symIndex = info >> 32;
    std::ostringstream oss;
    oss << std::hex << symIndex;
    return oss.str();
}

std::string SceDynlibParser::_getLibraryNameForRelocation(
    const std::vector<LibraryImport>& imports, const std::uint64_t info) const {
    std::uint32_t libIndex = static_cast<std::uint32_t>((info & LIB_INDEX_MASK) >> LIB_INDEX_SHIFT);
    if (libIndex == 0 || libIndex > imports.size()) {
        return "";
    }
    return imports[libIndex - 1].LibraryName;
}

std::vector<NidReference> SceDynlibParser::ExtractNidReferences(
    const std::vector<RelocationEntry>& relocations,
    const std::vector<LibraryImport>& imports) {
    std::vector<NidReference> refs;
    refs.reserve(relocations.size());

    for (std::size_t i = 0; i < relocations.size(); ++i) {
        const auto& reloc = relocations[i];
        std::uint32_t relocType = static_cast<std::uint32_t>(reloc.Info & RELOC_TYPE_MASK);

        std::string nid = _extractNidFromRelocationInfo(reloc.Info);
        std::string library = _getLibraryNameForRelocation(imports, reloc.Info);

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
