#include "ElfImageBuilder.hpp"

#include <Testing/Test.hpp>
#include <relinker/guest/GuestImage.hpp>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace Testing;
using namespace RelinkerUnitTests;
using Relinker::GuestImage;
using Relinker::GuestImageReader;
using Domain::RelinkerException;

constexpr std::size_t ImageSize = 0x1000;
constexpr std::uint64_t DynamicOffset = 0x200;
constexpr std::uint64_t StringsOffset = 0x400;
constexpr std::uint64_t SymbolsOffset = 0x500;
constexpr std::uint64_t CodeAddress = 0x800;
constexpr std::uint64_t LibcName = 1;
constexpr std::uint64_t SonameName = 10;
constexpr std::uint64_t ModuleName = 21;
constexpr std::uint64_t SymbolName = 28;
constexpr std::uint64_t StringsSize = 32;
constexpr std::uint64_t ModuleExportTag = 0x6100000d;
constexpr std::uint64_t SceSymbolTableSizeTag = 0x6100003f;
constexpr std::uint64_t SceStringTableTag = 0x61000035;
constexpr std::uint64_t MaxValue = std::numeric_limits<std::uint64_t>::max();
constexpr std::size_t LoadHeaderOffset = ElfHeaderSize;
constexpr std::size_t DynamicHeaderOffset = ElfHeaderSize + ProgramHeaderSize;
constexpr char ModulePath[] = "libfoo.sprx";

std::vector<DynamicEntry> guestTags() {
    return {
        {1, LibcName},
        {14, SonameName},
        {ModuleExportTag, (std::uint64_t{1} << 48) | ModuleName},
        {5, StringsOffset},
        {10, StringsSize},
        {6, SymbolsOffset},
        {11, SymbolSize},
        {SceSymbolTableSizeTag, 2 * SymbolSize},
    };
}

std::vector<SymbolSpec> guestSymbols() {
    return {{0, 0, 0, 0, 0, 0}, {static_cast<std::uint32_t>(SymbolName), 0x12, 0, 1, CodeAddress, 0x10}};
}

Bytes guestImage(std::vector<DynamicEntry> tags, const std::vector<SymbolSpec>& symbols, const bool terminate) {
    if (terminate) tags.push_back({0, 0});
    const auto dynamicSize = tags.size() * DynamicEntrySize;
    return ElfImageBuilder(ImageSize)
        .AddProgramHeader({PtLoad, PfRead | PfWrite | PfExecute, 0, 0, ImageSize, ImageSize, 0x1000})
        .AddProgramHeader({PtDynamic, PfRead | PfWrite, DynamicOffset, DynamicOffset, dynamicSize, dynamicSize, 8})
        .Data(DynamicOffset, EncodeDynamic(tags))
        .Data(StringsOffset, StringTable({"libc.prx", "libfoo.prx", "libFoo", "foo"}))
        .Data(SymbolsOffset, EncodeSymbols(symbols))
        .Build();
}

Bytes guestImage(const std::vector<DynamicEntry>& tags) {
    return guestImage(tags, guestSymbols(), true);
}

Bytes guestImage() {
    return guestImage(guestTags());
}

std::vector<DynamicEntry> replaceTag(std::vector<DynamicEntry> tags, const std::uint64_t tag, const std::uint64_t value) {
    for (auto& entry : tags) {
        if (entry.Tag == tag) entry.Value = value;
    }
    return tags;
}

std::vector<DynamicEntry> withoutTag(std::vector<DynamicEntry> tags, const std::uint64_t tag) {
    std::erase_if(tags, [&](const DynamicEntry& entry) { return entry.Tag == tag; });
    return tags;
}

std::vector<DynamicEntry> withTag(std::vector<DynamicEntry> tags, const std::uint64_t tag, const std::uint64_t value) {
    tags.push_back({tag, value});
    return tags;
}

void requireReadError(const Bytes& image, const std::string& expectedMessage, const std::string& context) {
    RequireThrowsWithMessage<RelinkerException>([&] { (void)GuestImageReader().Read(ModulePath, image); },
        std::string(ModulePath) + ": " + expectedMessage, context);
}

void requireModuleNamesError(const Bytes& image, const std::string& expectedMessage, const std::string& context) {
    RequireThrowsWithMessage<RelinkerException>([&] { (void)GuestImageReader().ReadModuleNames(image); }, expectedMessage, context);
}

const Case validGuestParses{"GuestImageReader_Read_ValidModule_ParsesMetadata", [] {
    const auto image = guestImage();

    const auto guest = GuestImageReader().Read(ModulePath, image);

    RequireEqual(guest.OutputName, std::string("libfoo.sprx.guest.prx"), "output name");
    RequireEqual(guest.Soname, std::string("libfoo.prx"), "soname");
    Require(guest.ModuleNames == std::vector<std::string>{"libFoo"}, "module names");
    Require(guest.Dependencies == std::vector<std::string>{"libc.prx"}, "dependencies");
    RequireEqual(guest.Headers.size(), std::size_t{2}, "program header count");
    RequireEqual(guest.Symbols.size(), std::size_t{2}, "symbol count");
    RequireEqual(guest.Symbols[1].Name, std::string("foo"), "symbol name");
    RequireEqual(guest.Symbols[1].Value, CodeAddress, "symbol value");
    RequireEqual(guest.Symbols[1].Section, 1, "symbol section");
    Require(guest.Dynamic.DynStrData == Bytes{0, 'f', 'o', 'o', 0}, "rebuilt string table");
    RequireEqual(guest.Dynamic.DynSymData.size(), 2 * SymbolSize, "symbol table size");
    RequireEqual(Read<std::uint32_t>(guest.Dynamic.DynSymData, SymbolSize), 1u, "rewritten symbol name offset");
    Require(guest.Dynamic.RelaData.empty() && guest.Dynamic.RelaPltData.empty(), "unexpected relocations");
    Require(guest.Bytes == image, "image bytes were not preserved");
}};

const Case shortFileRejected{"GuestImageReader_Read_FileShorterThanHeader_ThrowsRangeError", [] {
    requireReadError(Bytes(10, 0), "ELF range exceeds file: offset=0x0, size=0x40, fileSize=0xa", "10-byte file");
    requireReadError(Bytes{}, "ELF range exceeds file: offset=0x0, size=0x40, fileSize=0x0", "empty file");
}};

const Case unsupportedIdentRejected{"GuestImageReader_Read_NonX64LittleEndianElf_Throws", [] {
    const std::pair<std::size_t, std::uint8_t> cases[] = {{0, 0x7e}, {4, 1}, {5, 2}, {6, 0}, {18, 3}, {20, 0}};
    for (const auto& [index, value] : cases) {
        auto image = guestImage();
        image[index] = value;

        requireReadError(image, "Expected little-endian ELF64 x86-64", "byte " + std::to_string(index) + "=" + std::to_string(value));
    }
}};

const Case executableRejected{"GuestImageReader_Read_ExecutableType_ThrowsNotSharedModule", [] {
    auto image = guestImage();
    Write<std::uint16_t>(image, 0x10, 2);

    requireReadError(image, "ELF is not a shared module", "ET_EXEC");
}};

const Case invalidProgramHeaderTableRejected{"GuestImageReader_Read_InvalidProgramHeaderTableShape_Throws", [] {
    const std::pair<std::size_t, std::uint16_t> cases[] = {{0x34, 52}, {0x36, 64}, {0x36, 0}, {0x38, 0}};
    for (const auto& [offset, value] : cases) {
        auto image = guestImage();
        Write<std::uint16_t>(image, offset, value);

        requireReadError(image, "Invalid ELF program header table", "field " + std::to_string(offset) + "=" + std::to_string(value));
    }
}};

const Case programHeaderTablePastEndRejected{"GuestImageReader_Read_ProgramHeaderTablePastEnd_ThrowsRangeError", [] {
    auto image = guestImage();
    Write<std::uint16_t>(image, 0x38, 0xffff);

    requireReadError(image, "ELF range exceeds file: offset=0x40, size=0x37ffc8, fileSize=0x1000", "count 0xffff");
}};

const Case programHeaderOffsetNearMaximumRejected{"GuestImageReader_Read_ProgramHeaderOffsetNearUint64Max_ThrowsRangeError", [] {
    auto image = guestImage();
    Write<std::uint64_t>(image, 0x20, MaxValue - 8);

    requireReadError(image, "ELF range exceeds file: offset=0xfffffffffffffff7, size=0x70, fileSize=0x1000", "offset near max");
}};

const Case segmentPastEndRejected{"GuestImageReader_Read_SegmentFileRangePastEnd_ThrowsRangeError", [] {
    auto image = guestImage();
    Write<std::uint64_t>(image, DynamicHeaderOffset + 0x08, MaxValue - 15);

    requireReadError(image, "ELF range exceeds file: offset=0xfffffffffffffff0, size=0x90, fileSize=0x1000", "dynamic offset near max");
}};

const Case loadFileSizeAboveMemorySizeRejected{"GuestImageReader_Read_LoadFileSizeAboveMemorySize_Throws", [] {
    auto image = guestImage();
    Write<std::uint64_t>(image, LoadHeaderOffset + 0x28, ImageSize - 1);

    requireReadError(image, "Invalid load segment", "filesz > memsz");
}};

const Case loadAddressOverflowRejected{"GuestImageReader_Read_LoadEndOverflowsUint64_Throws", [] {
    auto image = guestImage();
    Write<std::uint64_t>(image, LoadHeaderOffset + 0x10, MaxValue - 0xfff);

    requireReadError(image, "Invalid load segment", "vaddr + memsz overflows");
}};

const Case misalignedLoadRejected{"GuestImageReader_Read_LoadAlignmentNotPowerOfTwo_Throws", [] {
    auto image = guestImage();
    Write<std::uint64_t>(image, LoadHeaderOffset + 0x30, 0x1800);

    requireReadError(image, "Invalid load segment alignment", "alignment 0x1800");
}};

const Case missingDynamicRejected{"GuestImageReader_Read_MissingOrMisalignedDynamic_Throws", [] {
    auto withoutDynamic = guestImage();
    Write<std::uint32_t>(withoutDynamic, DynamicHeaderOffset, 0);
    requireReadError(withoutDynamic, "Missing or invalid ELF load/dynamic segments", "no PT_DYNAMIC");

    auto misaligned = guestImage();
    Write<std::uint64_t>(misaligned, DynamicHeaderOffset + 0x20, 9 * DynamicEntrySize - 8);
    requireReadError(misaligned, "Missing or invalid ELF load/dynamic segments", "dynamic size not multiple of 16");
}};

const Case unterminatedDynamicRejected{"GuestImageReader_Read_DynamicWithoutNull_Throws", [] {
    requireReadError(guestImage(guestTags(), guestSymbols(), false), "Unterminated dynamic segment", "no DT_NULL");
}};

const Case duplicateDynamicTagRejected{"GuestImageReader_Read_DuplicateDynamicTag_Throws", [] {
    requireReadError(guestImage(withTag(guestTags(), 5, StringsOffset)), "Duplicate dynamic tag 5", "two DT_STRTAB");
}};

const Case unsupportedDynamicTagRejected{"GuestImageReader_Read_UnsupportedDynamicTag_Throws", [] {
    requireReadError(guestImage(withTag(guestTags(), 16, 0)), "Unsupported dynamic tag 16", "DT_SYMBOLIC");
}};

const Case unmappedStringTableRejected{"GuestImageReader_Read_StringTableOutsideLoads_Throws", [] {
    requireReadError(guestImage(replaceTag(guestTags(), 5, 0x5000)), "Unmapped ELF address 20480", "DT_STRTAB past image");
    requireReadError(guestImage(replaceTag(guestTags(), 10, MaxValue)), "Unmapped ELF address 1024", "DT_STRSZ UINT64_MAX");
}};

const Case sceStringTableWithoutDynlibRejected{"GuestImageReader_Read_SceStringTableWithoutDynlibData_Throws", [] {
    const auto tags = withTag(withoutTag(guestTags(), 5), SceStringTableTag, 0);

    requireReadError(guestImage(tags), "SCE table exceeds dynamic data segment", "DT_SCE_STRTAB without PT_SCE_DYNLIBDATA");
}};

const Case ambiguousStringTableRejected{"GuestImageReader_Read_StandardAndSceStringTable_Throws", [] {
    requireReadError(guestImage(withTag(guestTags(), SceStringTableTag, 0)), "Missing or ambiguous dynamic tag 5", "both string table tags");
}};

const Case invalidStringTableRejected{"GuestImageReader_Read_StringTableMissingLeadingNul_Throws", [] {
    requireReadError(guestImage(replaceTag(guestTags(), 5, StringsOffset + 1)), "Invalid string or symbol table", "string table starts at 'l'");
    requireReadError(guestImage(replaceTag(guestTags(), 11, 16)), "Invalid string or symbol table", "DT_SYMENT 16");
}};

const Case stringOffsetOutOfRangeRejected{"GuestImageReader_Read_NeededStringOffsetOutOfRange_Throws", [] {
    requireReadError(guestImage(replaceTag(guestTags(), 1, StringsSize)), "String offset out of bounds", "DT_NEEDED == strsz");
    requireReadError(guestImage(replaceTag(guestTags(), 1, MaxValue)), "String offset out of bounds", "DT_NEEDED UINT64_MAX");
}};

const Case unterminatedStringRejected{"GuestImageReader_Read_StringRunsPastTableEnd_Throws", [] {
    requireReadError(guestImage(replaceTag(guestTags(), 10, StringsSize - 1)), "Unterminated ELF string", "strsz cuts 'foo'");
}};

const Case invalidSymbolCountRejected{"GuestImageReader_Read_InvalidSymbolCount_Throws", [] {
    requireReadError(guestImage(replaceTag(guestTags(), SceSymbolTableSizeTag, 0)), "Invalid dynamic symbol count", "zero");
    requireReadError(guestImage(replaceTag(guestTags(), SceSymbolTableSizeTag, 25)), "Invalid dynamic symbol count", "not multiple of 24");
    requireReadError(guestImage(withoutTag(guestTags(), SceSymbolTableSizeTag)), "Missing dynamic symbol count", "missing");
    requireReadError(guestImage(replaceTag(guestTags(), SceSymbolTableSizeTag, 0x1000 / SymbolSize * SymbolSize)), "Unmapped ELF address 1280", "past end");
}};

const Case invalidNullSymbolRejected{"GuestImageReader_Read_NonZeroNullSymbol_Throws", [] {
    auto symbols = guestSymbols();
    symbols[0].Value = 1;

    requireReadError(guestImage(guestTags(), symbols, true), "Invalid null symbol", "null symbol value 1");
}};

const Case unmappedSymbolRejected{"GuestImageReader_Read_SymbolOutsideLoads_Throws", [] {
    auto symbols = guestSymbols();
    symbols[1].Value = MaxValue - 4;

    requireReadError(guestImage(guestTags(), symbols, true), "Unmapped or inaccessible guest address " + std::to_string(MaxValue - 4), "symbol near max");
}};

const Case duplicateModuleIdRejected{"GuestImageReader_Read_DuplicateExportModuleId_Throws", [] {
    const auto tags = withTag(guestTags(), ModuleExportTag, (std::uint64_t{1} << 48) | SonameName);

    requireReadError(guestImage(tags), "Duplicate export module ID", "two modules with ID 1");
}};

const Case moduleNamesParse{"GuestImageReader_ReadModuleNames_ValidModule_ReturnsExportedModules", [] {
    const auto names = GuestImageReader().ReadModuleNames(guestImage());

    Require(names == std::vector<std::string>{"libFoo"}, "module names");
}};

const Case moduleNamesForExecutableEmpty{"GuestImageReader_ReadModuleNames_ExecutableOrNoModules_ReturnsEmpty", [] {
    auto executable = guestImage();
    Write<std::uint16_t>(executable, 0x10, 2);
    Require(GuestImageReader().ReadModuleNames(executable).empty(), "executable produced module names");

    Require(GuestImageReader().ReadModuleNames(guestImage(withoutTag(guestTags(), ModuleExportTag))).empty(), "module without exports produced names");
}};

const Case moduleNamesShortOrForeignRejected{"GuestImageReader_ReadModuleNames_ShortOrForeignFile_Throws", [] {
    requireModuleNamesError(Bytes(63, 0), "Expected little-endian ELF64 x86-64 module metadata", "63-byte file");
    auto foreign = guestImage();
    Write<std::uint16_t>(foreign, 0x12, 183);
    requireModuleNamesError(foreign, "Expected little-endian ELF64 x86-64 module metadata", "aarch64");
}};

const Case moduleNamesProgramHeadersRejected{"GuestImageReader_ReadModuleNames_ProgramHeaderTablePastEnd_Throws", [] {
    auto image = guestImage();
    Write<std::uint64_t>(image, 0x20, MaxValue - 8);
    requireModuleNamesError(image, "Module metadata exceeds file", "offset near max");

    auto badEntrySize = guestImage();
    Write<std::uint16_t>(badEntrySize, 0x36, 64);
    requireModuleNamesError(badEntrySize, "Invalid module metadata program headers", "entry size 64");
}};

const Case moduleNamesStringOffsetRejected{"GuestImageReader_ReadModuleNames_NameOffsetOutOfRange_Throws", [] {
    const auto tags = replaceTag(guestTags(), ModuleExportTag, (std::uint64_t{1} << 48) | StringsSize);

    requireModuleNamesError(guestImage(tags), "Module metadata string offset out of bounds", "name offset == strsz");
}};

const Case moduleNamesUnterminatedDynamicRejected{"GuestImageReader_ReadModuleNames_DynamicWithoutNull_Throws", [] {
    requireModuleNamesError(guestImage(guestTags(), guestSymbols(), false), "Unterminated dynamic segment", "no DT_NULL");
}};

} // namespace
