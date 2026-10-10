#include "ElfImageBuilder.hpp"

#include <Testing/Test.hpp>
#include <relinker/parsing/ElfReader.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace Testing;
using namespace RelinkerUnitTests;
using Relinker::ElfReader;
using Relinker::ProgramHeader;
using Relinker::RelinkerException;
using Relinker::SectionHeader;

constexpr std::size_t ImageSize = 0x1000;
constexpr std::uint64_t CodeOffset = 0x800;
constexpr std::uint64_t CodeAddress = 0x400800;
constexpr std::uint64_t CodeSize = 0x100;
constexpr std::uint64_t DynamicOffset = 0x200;
constexpr std::uint64_t DynamicAddress = 0x600200;
constexpr std::uint64_t SectionTableOffset = 0x600;
constexpr std::uint64_t SectionNamesOffset = 0x700;
constexpr std::uint64_t MaxValue = std::numeric_limits<std::uint64_t>::max();
constexpr std::size_t CodeHeaderOffset = ElfHeaderSize;
constexpr std::size_t TextSectionHeaderOffset = SectionTableOffset + SectionHeaderSize;
constexpr std::size_t NamesSectionHeaderOffset = SectionTableOffset + 2 * SectionHeaderSize;

Bytes validImage() {
    const auto names = StringTable({".shstrtab", ".text"});
    Bytes code(CodeSize, 0x90);
    return ElfImageBuilder(ImageSize)
        .Entry(CodeAddress)
        .AddProgramHeader({PtLoad, PfRead | PfExecute, CodeOffset, CodeAddress, CodeSize, CodeSize, 0x1000})
        .AddProgramHeader({PtDynamic, PfRead | PfWrite, DynamicOffset, DynamicAddress, 3 * DynamicEntrySize, 3 * DynamicEntrySize, 8})
        .AddProgramHeader({PtLoad, PfRead | PfWrite, DynamicOffset, DynamicAddress, 0x100, 0x100, 0x1000})
        .SectionHeaderTable(SectionTableOffset, 2)
        .AddSectionHeader({0, 0, 0, 0})
        .AddSectionHeader({11, 1, CodeOffset, 0x10})
        .AddSectionHeader({1, 3, SectionNamesOffset, names.size()})
        .Data(DynamicOffset, EncodeDynamic({{5, 0x1234}, {10, 7}, {0, 0}}))
        .Data(SectionNamesOffset, names)
        .Data(CodeOffset, code)
        .Build();
}

ProgramHeader segment(const std::uint64_t offset, const std::uint64_t size) {
    ProgramHeader header{};
    header.Type = PtDynamic;
    header.Offset = offset;
    header.FileSize = size;
    return header;
}

template<typename TOperation>
RelinkerException requireRelinkerError(const TOperation& operation, const std::string& expectedMessage, const std::string& context) {
    const auto error = RequireThrows<RelinkerException>(operation, context);
    RequireEqual(std::string(error.what()), expectedMessage, context);
    return error;
}

const Case validHeaderParses{"ElfReader_ReadHeader_ValidImage_ParsesAllFields", [] {
    const ElfReader reader(validImage());

    const auto header = reader.ReadHeader();

    RequireEqual(header.Machine, 62, "machine");
    RequireEqual(header.Type, 3, "type");
    RequireEqual(header.EntryPoint, CodeAddress, "entry point");
    RequireEqual(header.ProgramHeaderOffset, ElfHeaderSize, "program header offset");
    RequireEqual(header.ProgramHeaderEntrySize, ProgramHeaderSize, "program header entry size");
    RequireEqual(header.ProgramHeaderCount, 3, "program header count");
    RequireEqual(header.SectionHeaderOffset, SectionTableOffset, "section header offset");
    RequireEqual(header.SectionHeaderEntrySize, SectionHeaderSize, "section header entry size");
    RequireEqual(header.SectionHeaderCount, 3, "section header count");
    RequireEqual(header.SectionHeaderStringIndex, 2, "section name index");
    RequireEqual(reader.GetFileSize(), ImageSize, "file size");
}};

const Case emptyFileRejected{"ElfReader_ReadHeader_EmptyFile_ThrowsTooSmall", [] {
    const ElfReader reader(Bytes{});

    requireRelinkerError([&] { (void)reader.ReadHeader(); }, "File too small for ELF header", "empty file");
}};

const Case truncatedHeaderRejected{"ElfReader_ReadHeader_FileShorterThanHeader_ThrowsTooSmall", [] {
    auto image = validImage();
    image.resize(ElfHeaderSize - 1);
    const ElfReader reader(image);

    requireRelinkerError([&] { (void)reader.ReadHeader(); }, "File too small for ELF header", "63-byte file");
}};

const Case selfContainerRejected{"ElfReader_ReadHeader_SelfContainerMagic_ThrowsSelfMessage", [] {
    for (const std::uint32_t magic : {0x1d3d154fu, 0xeef51454u}) {
        auto image = validImage();
        Write<std::uint32_t>(image, 0, magic);
        const ElfReader reader(image);

        requireRelinkerError([&] { (void)reader.ReadHeader(); }, "The input is a SELF container, not an ELF", "SELF magic " + std::to_string(magic));
    }
}};

const Case badMagicRejected{"ElfReader_ReadHeader_BadMagic_ThrowsWithHexMagic", [] {
    auto image = validImage();
    Write<std::uint32_t>(image, 0, 0x00905a4du);
    const ElfReader reader(image);

    requireRelinkerError([&] { (void)reader.ReadHeader(); }, "Invalid ELF magic number: 4d 5a 90 00", "PE magic");
}};

const Case unsupportedIdentRejected{"ElfReader_ReadHeader_NonElf64LittleEndianIdent_Throws", [] {
    const std::pair<std::size_t, std::uint8_t> cases[] = {{4, 1}, {4, 0}, {5, 2}, {5, 0}, {6, 0}, {6, 2}};
    for (const auto& [index, value] : cases) {
        auto image = validImage();
        image[index] = value;
        const ElfReader reader(image);

        requireRelinkerError([&] { (void)reader.ReadHeader(); }, "Expected little-endian ELF64 version 1",
            "ident byte " + std::to_string(index) + "=" + std::to_string(value));
    }
}};

const Case validProgramHeadersParse{"ElfReader_ReadProgramHeaders_ValidImage_ParsesEntries", [] {
    const ElfReader reader(validImage());

    const auto headers = reader.ReadProgramHeaders();

    RequireEqual(headers.size(), std::size_t{3}, "program header count");
    RequireEqual(headers[0].Type, PtLoad, "code type");
    RequireEqual(headers[0].Flags, PfRead | PfExecute, "code flags");
    RequireEqual(headers[0].Offset, CodeOffset, "code offset");
    RequireEqual(headers[0].MappedAddress, CodeAddress, "code address");
    RequireEqual(headers[0].PhysicalAddress, CodeAddress, "code physical address");
    RequireEqual(headers[0].FileSize, CodeSize, "code file size");
    RequireEqual(headers[0].MemorySize, CodeSize, "code memory size");
    RequireEqual(headers[0].Alignment, std::uint64_t{0x1000}, "code alignment");
    RequireEqual(headers[1].Type, PtDynamic, "dynamic type");
    RequireEqual(headers[1].FileSize, 3 * DynamicEntrySize, "dynamic size");
}};

const Case programHeaderEntrySizeMismatchRejected{"ElfReader_ReadProgramHeaders_EntrySizeMismatch_Throws", [] {
    for (const std::uint16_t size : {std::uint16_t{0}, std::uint16_t{55}, std::uint16_t{64}}) {
        auto image = validImage();
        Write<std::uint16_t>(image, 0x36, size);
        const ElfReader reader(image);

        const auto error = requireRelinkerError([&] { (void)reader.ReadProgramHeaders(); },
            "Invalid ELF program header entry size: expected 56 bytes", "entry size " + std::to_string(size));
        RequireEqual(error.FailureOffset, std::uint64_t{0x36}, "failure offset");
    }
}};

const Case emptyProgramHeaderTableAccepted{"ElfReader_ReadProgramHeaders_ZeroCountWithZeroEntrySize_ReturnsEmpty", [] {
    auto image = validImage();
    Write<std::uint16_t>(image, 0x36, 0);
    Write<std::uint16_t>(image, 0x38, 0);
    Write<std::uint64_t>(image, 0x20, MaxValue);
    const ElfReader reader(image);

    Require(reader.ReadProgramHeaders().empty(), "empty program header table produced entries");
}};

const Case programHeaderTablePastEndRejected{"ElfReader_ReadProgramHeaders_TableOffsetPastEnd_Throws", [] {
    auto image = validImage();
    Write<std::uint64_t>(image, 0x20, 0x2000);
    const ElfReader reader(image);

    const auto error = requireRelinkerError([&] { (void)reader.ReadProgramHeaders(); }, "FileByteOffset out of bounds", "table past end");
    RequireEqual(error.FailureOffset, std::uint64_t{0x2000}, "failure offset");
}};

const Case programHeaderTableNearMaximumRejected{"ElfReader_ReadProgramHeaders_TableOffsetNearUint64Max_Throws", [] {
    for (const std::uint64_t offset : {MaxValue, MaxValue - 3, MaxValue - ProgramHeaderSize + 1}) {
        auto image = validImage();
        Write<std::uint64_t>(image, 0x20, offset);
        const ElfReader reader(image);

        requireRelinkerError([&] { (void)reader.ReadProgramHeaders(); }, "FileByteOffset out of bounds", "table offset " + std::to_string(offset));
    }
}};

const Case programHeaderCountExceedingFileRejected{"ElfReader_ReadProgramHeaders_CountExceedsFile_Throws", [] {
    auto image = validImage();
    Write<std::uint16_t>(image, 0x38, 0xffff);
    const ElfReader reader(image);

    requireRelinkerError([&] { (void)reader.ReadProgramHeaders(); }, "FileByteOffset out of bounds", "count 0xffff");
}};

const Case truncatedProgramHeaderRejected{"ElfReader_ReadProgramHeaders_LastEntryTruncated_Throws", [] {
    auto image = validImage();
    Write<std::uint64_t>(image, 0x20, ImageSize - 3 * ProgramHeaderSize + 8);
    const ElfReader reader(image);

    requireRelinkerError([&] { (void)reader.ReadProgramHeaders(); }, "FileByteOffset out of bounds", "truncated last entry");
}};

const Case validSectionHeadersParse{"ElfReader_ReadSectionHeaders_ValidImage_ResolvesNames", [] {
    const ElfReader reader(validImage());

    const auto headers = reader.ReadSectionHeaders();

    RequireEqual(headers.size(), std::size_t{3}, "section count");
    RequireEqual(headers[0].Name, std::string{}, "null section name");
    RequireEqual(headers[1].Name, std::string{".text"}, "text section name");
    RequireEqual(headers[1].Offset, CodeOffset, "text offset");
    RequireEqual(headers[1].SectionSize, std::uint64_t{0x10}, "text size");
    RequireEqual(headers[2].Name, std::string{".shstrtab"}, "names section name");
    RequireEqual(reader.ReadSection(headers[1]), Bytes(0x10, 0x90), "text bytes");
}};

const Case sectionHeaderTablePastEndRejected{"ElfReader_ReadSectionHeaders_TableOffsetPastEnd_Throws", [] {
    auto image = validImage();
    Write<std::uint64_t>(image, 0x28, ImageSize - SectionHeaderSize + 1);
    const ElfReader reader(image);

    requireRelinkerError([&] { (void)reader.ReadSectionHeaders(); }, "FileByteOffset out of bounds", "section table past end");
}};

const Case sectionHeaderEntrySizeMismatchRejected{"ElfReader_ReadSectionHeaders_EntrySizeMismatch_Throws", [] {
    for (const std::uint16_t size : {std::uint16_t{0}, std::uint16_t{32}}) {
        auto image = validImage();
        Write<std::uint16_t>(image, 0x3a, size);
        const ElfReader reader(image);

        RequireThrows<RelinkerException>([&] { (void)reader.ReadSectionHeaders(); }, "section entry size " + std::to_string(size));
    }
}};

const Case sectionNameIndexBeyondCountRejected{"ElfReader_ReadSectionHeaders_NameTableIndexBeyondCount_Throws", [] {
    auto image = validImage();
    Write<std::uint16_t>(image, 0x3e, 7);
    const ElfReader reader(image);

    RequireThrows<RelinkerException>([&] { (void)reader.ReadSectionHeaders(); }, "section name index 7 of 3");
}};

const Case sectionNameOffsetWrapRejected{"ElfReader_ReadSectionHeaders_NameOffsetWrapsPastUint64Max_Throws", [] {
    auto image = validImage();
    Write<std::uint64_t>(image, NamesSectionHeaderOffset + 0x18, MaxValue);
    Write<std::uint32_t>(image, TextSectionHeaderOffset, 1);
    const ElfReader reader(image);

    RequireThrows<RelinkerException>([&] { (void)reader.ReadSectionHeaders(); }, "name table offset UINT64_MAX with name offset 1");
}};

const Case validDynamicTagsParse{"ElfReader_ReadDynamicTags_ValidSegment_StopsAtNull", [] {
    auto image = validImage();
    const ElfReader reader(image);

    const auto tags = reader.ReadDynamicTags(segment(DynamicOffset, 4 * DynamicEntrySize));

    RequireEqual(tags.size(), std::size_t{2}, "tag count");
    RequireEqual(tags[0].Tag, std::int64_t{5}, "first tag");
    RequireEqual(tags[0].Value, std::uint64_t{0x1234}, "first value");
    RequireEqual(tags[1].Tag, std::int64_t{10}, "second tag");
    RequireEqual(tags[1].Value, std::uint64_t{7}, "second value");
}};

const Case unterminatedDynamicRejected{"ElfReader_ReadDynamicTags_MissingNullTerminator_Throws", [] {
    for (const std::uint64_t size : {std::uint64_t{0}, 2 * DynamicEntrySize}) {
        const ElfReader reader(validImage());

        const auto error = requireRelinkerError([&] { (void)reader.ReadDynamicTags(segment(DynamicOffset, size)); },
            "Unterminated dynamic segment", "dynamic size " + std::to_string(size));
        RequireEqual(error.FailureOffset, DynamicOffset, "failure offset");
    }
}};

const Case unalignedDynamicSizeRejected{"ElfReader_ReadDynamicTags_SizeNotMultipleOfEntry_Throws", [] {
    const ElfReader reader(validImage());

    requireRelinkerError([&] { (void)reader.ReadDynamicTags(segment(DynamicOffset, 3 * DynamicEntrySize - 1)); },
        "Invalid dynamic segment size", "dynamic size 47");
}};

const Case dynamicPastEndRejected{"ElfReader_ReadDynamicTags_SegmentPastEnd_Throws", [] {
    const std::pair<std::uint64_t, std::uint64_t> cases[] = {
        {ImageSize - DynamicEntrySize, 2 * DynamicEntrySize},
        {ImageSize + DynamicEntrySize, DynamicEntrySize},
        {DynamicEntrySize, MaxValue - 15},
        {MaxValue - 15, DynamicEntrySize},
    };
    for (const auto& [offset, size] : cases) {
        const ElfReader reader(validImage());

        requireRelinkerError([&] { (void)reader.ReadDynamicTags(segment(offset, size)); }, "Dynamic segment out of bounds",
            "dynamic offset " + std::to_string(offset) + " size " + std::to_string(size));
    }
}};

const Case translateMappedAddress{"ElfReader_TranslateVirtualAddress_AddressInLoad_ReturnsFileOffset", [] {
    const ElfReader reader(validImage());

    RequireEqual(reader.TranslateVirtualAddress(CodeAddress), CodeOffset, "segment start");
    RequireEqual(reader.TranslateVirtualAddress(CodeAddress + CodeSize - 1), CodeOffset + CodeSize - 1, "segment last byte");
}};

const Case translateUnmappedAddressRejected{"ElfReader_TranslateVirtualAddress_AddressOutsideLoads_Throws", [] {
    for (const std::uint64_t address : {CodeAddress + CodeSize, CodeAddress - 1, std::uint64_t{0}, MaxValue}) {
        const ElfReader reader(validImage());

        const auto error = requireRelinkerError([&] { (void)reader.TranslateVirtualAddress(address); },
            "Virtual address not mapped by any PT_LOAD segment", "address " + std::to_string(address));
        RequireEqual(error.FailureOffset, address, "failure offset");
    }
}};

const Case translateWrappingSegmentRejected{"ElfReader_TranslateVirtualAddress_SegmentEndWrapsUint64_Throws", [] {
    auto image = validImage();
    Write<std::uint64_t>(image, CodeHeaderOffset + 0x10, MaxValue - 0xf);
    const ElfReader reader(image);

    RequireThrows<RelinkerException>([&] { (void)reader.TranslateVirtualAddress(MaxValue - 1); }, "segment end wraps");
}};

const Case translateWrappingFileOffsetRejected{"ElfReader_TranslateVirtualAddress_FileOffsetWrapsUint64_Throws", [] {
    auto image = validImage();
    Write<std::uint64_t>(image, CodeHeaderOffset + 0x08, MaxValue - 7);
    const ElfReader reader(image);

    RequireThrows<RelinkerException>([&] { (void)reader.TranslateVirtualAddress(CodeAddress + 0x10); }, "file offset wraps to 8");
}};

const Case translateBeyondFileRejected{"ElfReader_TranslateVirtualAddress_SegmentBeyondFile_Throws", [] {
    auto image = validImage();
    Write<std::uint64_t>(image, CodeHeaderOffset + 0x08, ImageSize - 0x80);
    const ElfReader reader(image);

    RequireThrows<RelinkerException>([&] { (void)reader.TranslateVirtualAddress(CodeAddress + 0x90); }, "translated offset past end of file");
}};

const Case readSegmentReturnsBytes{"ElfReader_ReadSegment_ValidHeader_ReturnsFileBytes", [] {
    const ElfReader reader(validImage());

    const auto bytes = reader.ReadSegment(reader.ReadProgramHeaders()[0]);

    RequireEqual(bytes, Bytes(CodeSize, 0x90), "segment bytes");
}};

const Case readSegmentOutOfRangeRejected{"ElfReader_ReadSegment_RangePastEndOrOverflowing_Throws", [] {
    const std::pair<std::uint64_t, std::uint64_t> cases[] = {
        {ImageSize - 1, 2}, {ImageSize + 1, 0}, {1, MaxValue}, {MaxValue, 1},
    };
    for (const auto& [offset, size] : cases) {
        const ElfReader reader(validImage());

        const auto error = requireRelinkerError([&] { (void)reader.ReadSegment(segment(offset, size)); }, "Segment offset out of bounds",
            "segment offset " + std::to_string(offset) + " size " + std::to_string(size));
        RequireEqual(error.FailureOffset, offset, "failure offset");
    }
}};

const Case readSectionOutOfRangeRejected{"ElfReader_ReadSection_RangePastEndOrOverflowing_Throws", [] {
    const std::pair<std::uint64_t, std::uint64_t> cases[] = {{ImageSize, 1}, {1, MaxValue}};
    for (const auto& [offset, size] : cases) {
        const ElfReader reader(validImage());
        SectionHeader header{};
        header.Offset = offset;
        header.SectionSize = size;

        requireRelinkerError([&] { (void)reader.ReadSection(header); }, "Section offset out of bounds",
            "section offset " + std::to_string(offset) + " size " + std::to_string(size));
    }
}};

const Case codeSegmentsFiltered{"ElfReader_ReadCodeSegments_MixedSegments_ReturnsExecutableLoadsOnly", [] {
    const ElfReader reader(validImage());

    const auto segments = reader.ReadCodeSegments();

    RequireEqual(segments.size(), std::size_t{1}, "code segment count");
    RequireEqual(segments[0].MappedAddress, CodeAddress, "code segment address");
}};

} // namespace
