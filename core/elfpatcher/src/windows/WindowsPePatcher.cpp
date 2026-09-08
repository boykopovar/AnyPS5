#include <elfpatcher/windows/WindowsElfPatcher.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include "WindowsPeFormat.hpp"

namespace Elfpatcher::Windows {

namespace {

MzHeader buildMzStub(std::uint32_t peOffset) {
    MzHeader h{};
    h.data[0] = 'M';
    h.data[1] = 'Z';
    std::memcpy(h.data + 0x3c, &peOffset, 4);
    return h;
}

void writeSectionHeader(std::vector<std::uint8_t>& buf, std::size_t off, const PeSection& sec) {
    std::memcpy(buf.data() + off, sec.name, 8);
    Io::WriteU32(buf, off + 8, sec.virtualSize);
    Io::WriteU32(buf, off + 12, sec.virtualAddress);
    Io::WriteU32(buf, off + 16, sec.rawSize);
    Io::WriteU32(buf, off + 20, sec.rawOffset);
    Io::WriteU32(buf, off + 24, 0);
    Io::WriteU32(buf, off + 28, 0);
    Io::WriteU16(buf, off + 32, 0);
    Io::WriteU16(buf, off + 34, 0);
    Io::WriteU32(buf, off + 36, sec.characteristics);
}

std::vector<std::uint8_t> buildRelocSection(
    const std::vector<std::uint8_t>& relaData,
    const std::uint64_t imageBase)
{
    constexpr std::size_t kRelaEntSize = 24;
    if (relaData.size() % kRelaEntSize != 0)
        throw Domain::RelinkerException("RELA data size is not a multiple of 24");

    std::map<std::uint32_t, std::vector<std::uint32_t>> pageToOffsets;

    for (std::size_t i = 0; i + kRelaEntSize <= relaData.size(); i += kRelaEntSize) {
        std::uint64_t rOffset, rInfo;
        std::int64_t rAddend;
        std::memcpy(&rOffset, relaData.data() + i, 8);
        std::memcpy(&rInfo, relaData.data() + i + 8, 8);
        std::memcpy(&rAddend, relaData.data() + i + 16, 8);

        const auto relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);
        const auto symIdx = static_cast<std::uint32_t>(rInfo >> 32);

        constexpr std::uint32_t kRelativeType = 8;
        if (relType != kRelativeType)
            continue;
        if (symIdx != 0)
            throw Domain::RelinkerException("R_X86_64_RELATIVE with non-zero symbol index");

        if (rOffset < imageBase)
            throw Domain::RelinkerException("RELATIVE relocation target vaddr below imageBase", rOffset);

        const std::uint64_t rva = rOffset - imageBase;
        if (rva > 0xFFFFFFFFull)
            throw Domain::RelinkerException("RELATIVE relocation RVA exceeds 32-bit range", rOffset);

        const auto rva32 = static_cast<std::uint32_t>(rva);
        const std::uint32_t page = rva32 & ~0xFFFu;
        const std::uint32_t pageOff = rva32 & 0xFFFu;
        pageToOffsets[page].push_back(pageOff);
    }

    std::vector<std::uint8_t> buf;
    for (auto& [page, offsets] : pageToOffsets) {
        std::sort(offsets.begin(), offsets.end());
        auto entryCount = static_cast<std::uint32_t>(offsets.size());
        if (entryCount % 2 != 0) {
            offsets.push_back(0);
            entryCount++;
        }
        const std::uint32_t blockSize = kRelocBlockHeaderSize + entryCount * kRelocEntrySize;
        Io::AppendU32(buf, page);
        Io::AppendU32(buf, blockSize);
        for (const std::uint32_t off : offsets) {
            const auto entry = static_cast<std::uint16_t>((kRelTypeDir64 << 12) | (off & 0xFFF));
            Io::AppendU16(buf, entry);
        }
    }
    return buf;
}


std::vector<std::uint8_t> buildImportSection(
    const std::vector<ImportDllBlock>& dlls,
    const std::uint32_t sectionRva,
    std::vector<std::pair<std::uint32_t, std::uint32_t>>& outGotPatches)
{
    if (dlls.empty())
        return {};

    const auto descriptorTableSize = static_cast<std::uint32_t>((dlls.size() + 1) * 20);

    std::vector<std::uint8_t> descriptorBuf;
    std::vector<std::uint8_t> dataBuf;

    const std::uint32_t dataBase = sectionRva + descriptorTableSize;

    for (const auto& dll : dlls) {
        const std::uint32_t dllNameRva = dataBase + static_cast<std::uint32_t>(dataBuf.size());
        Io::AppendString(dataBuf, dll.dllName);
        Io::AlignBuffer(dataBuf, 2);

        const std::uint32_t iltRva = dataBase + static_cast<std::uint32_t>(dataBuf.size());
        for (const auto& thunk : dll.thunks) {
            const std::uint32_t hintNameRva = dataBase + static_cast<std::uint32_t>(dataBuf.size())
                + static_cast<std::uint32_t>((dll.thunks.size() + 1) * 8);
            Io::AppendU64(dataBuf, hintNameRva);
        }
        Io::AppendU64(dataBuf, 0);

        for (const auto& thunk : dll.thunks) {
            Io::AppendU16(dataBuf, 0);
            Io::AppendString(dataBuf, thunk.symbolName);
            Io::AlignBuffer(dataBuf, 2);
        }

        const std::uint32_t iatRva = dataBase + static_cast<std::uint32_t>(dataBuf.size());
        for (std::size_t i = 0; i < dll.thunks.size(); ++i) {
            const std::uint32_t iatEntryRva = iatRva + static_cast<std::uint32_t>(i * 8);
            outGotPatches.emplace_back(dll.thunks[i].gotRva, iatEntryRva);
            Io::AppendU64(dataBuf, 0);
        }
        Io::AppendU64(dataBuf, 0);

        Io::AppendU32(descriptorBuf, iltRva);
        Io::AppendU32(descriptorBuf, 0);
        Io::AppendU32(descriptorBuf, 0);
        Io::AppendU32(descriptorBuf, dllNameRva);
        Io::AppendU32(descriptorBuf, iatRva);
    }

    Io::AppendU32(descriptorBuf, 0);
    Io::AppendU32(descriptorBuf, 0);
    Io::AppendU32(descriptorBuf, 0);
    Io::AppendU32(descriptorBuf, 0);
    Io::AppendU32(descriptorBuf, 0);

    std::vector<std::uint8_t> result;
    result.insert(result.end(), descriptorBuf.begin(), descriptorBuf.end());
    result.insert(result.end(), dataBuf.begin(), dataBuf.end());
    return result;
}

std::vector<std::uint8_t> buildEntryStub(std::uint64_t realEntryVaddr, std::uint64_t stubVaddr) {
    std::vector<std::uint8_t> s;
    const std::uint64_t callInsnVaddr = stubVaddr + s.size() + 5;
    const std::int64_t rel = static_cast<std::int64_t>(realEntryVaddr) - static_cast<std::int64_t>(callInsnVaddr);
    if (rel < std::int64_t(-0x80000000) || rel > std::int64_t(0x7FFFFFFF))
        throw Domain::RelinkerException("Entry stub: rel32 call target out of range");
    const auto rel32 = static_cast<std::int32_t>(rel);
    s.push_back(0xE8);
    s.push_back(static_cast<std::uint8_t>(rel32 & 0xFF));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 8) & 0xFF));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 16) & 0xFF));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 24) & 0xFF));
    s.push_back(0xC3);
    return s;
}

std::uint64_t readElfU64(const std::vector<std::uint8_t>& buf, std::size_t off) {
    if (off + 8 > buf.size())
        throw Domain::RelinkerException("ELF read out of bounds", off);
    std::uint64_t v;
    std::memcpy(&v, buf.data() + off, 8);
    return v;
}

std::uint32_t readElfU32(const std::vector<std::uint8_t>& buf, std::size_t off) {
    if (off + 4 > buf.size())
        throw Domain::RelinkerException("ELF read out of bounds", off);
    std::uint32_t v;
    std::memcpy(&v, buf.data() + off, 4);
    return v;
}

std::string readCStr(const std::vector<std::uint8_t>& buf, std::size_t off) {
    std::string s;
    while (off < buf.size() && buf[off] != 0)
        s.push_back(static_cast<char>(buf[off++]));
    return s;
}

}

WindowsPePatcher::WindowsPePatcher(
    std::shared_ptr<IEntryStubBuilder> entryStubBuilder,
    std::shared_ptr<IProgramHeaderLayoutBuilder> programHeaderLayoutBuilder,
    std::shared_ptr<ISectionHeaderTableBuilder> sectionHeaderTableBuilder,
    std::shared_ptr<Io::IByteWriter> byteWriter)
    : _entryStubBuilder(std::move(entryStubBuilder))
    , _programHeaderLayoutBuilder(std::move(programHeaderLayoutBuilder))
    , _sectionHeaderTableBuilder(std::move(sectionHeaderTableBuilder))
    , _byteWriter(std::move(byteWriter))
{
}

std::vector<std::uint8_t> WindowsPePatcher::Patch(
    const std::vector<std::uint8_t>& sourceElf,
    const std::vector<Domain::ProgramHeader>& originalHeaders,
    const Domain::SysVDynamicSection& dynamicSection,
    std::uint64_t originalPltGotVaddr,
    const std::string& runPath)
{
    if (sourceElf.size() < 64)
        throw Domain::RelinkerException("Source ELF too small");
    if (sourceElf[0] != 0x7f || sourceElf[1] != 'E' || sourceElf[2] != 'L' || sourceElf[3] != 'F')
        throw Domain::RelinkerException("Source is not an ELF file");

    const std::uint64_t elfEntryVaddr = readElfU64(sourceElf, 0x18);

    std::vector<LoadSegment> loadSegs;
    auto imageBase = std::uint64_t(-1);
    std::uint64_t imageTop = 0;

    for (const auto& ph : originalHeaders) {
        if (ph.Type != PT_LOAD) continue;
        if (ph.MemorySize == 0) continue;
        LoadSegment seg{};
        seg.vaddr = ph.MappedAddress;
        seg.memSize = ph.MemorySize;
        seg.fileOffset = ph.Offset;
        seg.fileSize = ph.FileSize;
        seg.flags = ph.Flags;
        loadSegs.push_back(seg);
        if (seg.vaddr < imageBase) imageBase = seg.vaddr;
        const std::uint64_t top = Io::AlignUp64(seg.vaddr + seg.memSize, kSectionAlignment);
        if (top > imageTop) imageTop = top;
    }

    if (loadSegs.empty())
        throw Domain::RelinkerException("No PT_LOAD segments found");
    if (imageBase == std::uint64_t(-1))
        throw Domain::RelinkerException("Could not determine image base");
    if (imageBase % kSectionAlignment != 0)
        throw Domain::RelinkerException("Image base is not section-aligned");

    std::uint32_t gotRva = 0;
    bool gotFound = false;
    if (originalPltGotVaddr != 0) {
        if (originalPltGotVaddr < imageBase)
            throw Domain::RelinkerException("GOT vaddr below image base");
        const std::uint64_t gotRva64 = originalPltGotVaddr - imageBase;
        if (gotRva64 > 0xFFFFFFFFull)
            throw Domain::RelinkerException("GOT RVA exceeds 32-bit range");
        gotRva = static_cast<std::uint32_t>(gotRva64);
        gotFound = true;
    }

    std::vector<std::uint8_t> imageBuf(static_cast<std::size_t>(imageTop - imageBase), 0);

    for (const auto& seg : loadSegs) {
        const std::uint64_t destOff = seg.vaddr - imageBase;
        if (destOff + seg.fileSize > imageBuf.size())
            throw Domain::RelinkerException("PT_LOAD segment exceeds image buffer", seg.vaddr);
        if (seg.fileOffset + seg.fileSize > sourceElf.size())
            throw Domain::RelinkerException("PT_LOAD segment file data out of bounds", seg.fileOffset);
        std::memcpy(imageBuf.data() + destOff, sourceElf.data() + seg.fileOffset, static_cast<std::size_t>(seg.fileSize));
    }

    const auto loadImageRva = static_cast<std::uint32_t>(kSizeOfPeHeaders);
    if (imageBase < loadImageRva)
        throw Domain::RelinkerException("Image base too small to accommodate PE headers");

    const std::uint64_t adjustedBase = imageBase - loadImageRva;
    if (adjustedBase % kSectionAlignment != 0)
        throw Domain::RelinkerException("Adjusted image base is not section-aligned");

    std::vector<ImportDllBlock> importDlls;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> gotPatches;

    {
        const std::vector<std::uint8_t>& relaPlt = dynamicSection.RelaPltData;
        const std::vector<std::uint8_t>& dynStr = dynamicSection.DynStrData;
        const std::vector<std::uint8_t>& dynSym = dynamicSection.DynSymData;

        constexpr std::size_t kRelaEnt = 24;
        constexpr std::size_t kSymEnt = 24;

        if (relaPlt.size() % kRelaEnt != 0)
            throw Domain::RelinkerException("RelaPlt size not multiple of 24");

        std::map<std::string, ImportDllBlock*> dllMap;

        for (std::size_t i = 0; i + kRelaEnt <= relaPlt.size(); i += kRelaEnt) {
            std::uint64_t rOffset, rInfo;
            std::memcpy(&rOffset, relaPlt.data() + i, 8);
            std::memcpy(&rInfo, relaPlt.data() + i + 8, 8);

            const auto symIdx = static_cast<std::uint32_t>(rInfo >> 32);
            const auto relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);

            constexpr std::uint32_t kJumpSlot = 7;
            if (relType != kJumpSlot)
                throw Domain::RelinkerException("Unexpected relocation type in RelaPlt");
            if (symIdx == 0)
                throw Domain::RelinkerException("JUMP_SLOT relocation with zero symbol index");

            const std::size_t symOff = static_cast<std::size_t>(symIdx) * kSymEnt;
            if (symOff + 4 > dynSym.size())
                throw Domain::RelinkerException("Symbol index out of dynSym bounds");

            std::uint32_t nameOff;
            std::memcpy(&nameOff, dynSym.data() + symOff, 4);
            if (nameOff >= dynStr.size())
                throw Domain::RelinkerException("Symbol name offset out of dynStr bounds");

            const std::string symName = readCStr(dynStr, nameOff);
            if (symName.empty())
                throw Domain::RelinkerException("Empty symbol name in JUMP_SLOT relocation");

            if (rOffset < imageBase)
                throw Domain::RelinkerException("JUMP_SLOT target vaddr below image base", rOffset);
            const std::uint64_t rva64 = rOffset - imageBase;
            if (rva64 > 0xFFFFFFFFull)
                throw Domain::RelinkerException("JUMP_SLOT target RVA exceeds 32-bit range");
            const auto targetGotRva = static_cast<std::uint32_t>(rva64);

            const std::string dllName = symName + ".dll";
            if (dllMap.find(dllName) == dllMap.end()) {
                importDlls.push_back({dllName, {}});
                dllMap[dllName] = &importDlls.back();
            }
            dllMap[dllName]->thunks.push_back({symName, targetGotRva});
        }

        const std::vector<std::uint8_t>& relaData = dynamicSection.RelaData;
        if (relaData.size() % kRelaEnt != 0)
            throw Domain::RelinkerException("RelaData size not multiple of 24");

        constexpr std::uint32_t kGlobDat = 6;
        constexpr std::uint32_t kAbs64 = 1;

        for (std::size_t i = 0; i + kRelaEnt <= relaData.size(); i += kRelaEnt) {
            std::uint64_t rOffset, rInfo;
            std::memcpy(&rOffset, relaData.data() + i, 8);
            std::memcpy(&rInfo, relaData.data() + i + 8, 8);

            const auto symIdx = static_cast<std::uint32_t>(rInfo >> 32);
            const auto relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);

            constexpr std::uint32_t kRelativeType = 8;
            if (relType == kRelativeType) continue;

            if (relType != kGlobDat && relType != kAbs64)
                throw Domain::RelinkerException("Unexpected relocation type in RelaData");
            if (symIdx == 0)
                throw Domain::RelinkerException("GLOB_DAT/ABS64 relocation with zero symbol index");

            const std::size_t symOff = static_cast<std::size_t>(symIdx) * kSymEnt;
            if (symOff + 4 > dynSym.size())
                throw Domain::RelinkerException("Symbol index out of dynSym bounds");

            std::uint32_t nameOff;
            std::memcpy(&nameOff, dynSym.data() + symOff, 4);
            if (nameOff >= dynStr.size())
                throw Domain::RelinkerException("Symbol name offset out of dynStr bounds");

            const std::string symName = readCStr(dynStr, nameOff);
            if (symName.empty())
                throw Domain::RelinkerException("Empty symbol name in GLOB_DAT/ABS64 relocation");

            if (rOffset < imageBase)
                throw Domain::RelinkerException("GLOB_DAT target vaddr below image base", rOffset);
            const std::uint64_t rva64 = rOffset - imageBase;
            if (rva64 > 0xFFFFFFFFull)
                throw Domain::RelinkerException("GLOB_DAT target RVA exceeds 32-bit range");
            const auto targetGotRva = static_cast<std::uint32_t>(rva64);

            const std::string dllName = symName + ".dll";
            if (dllMap.find(dllName) == dllMap.end()) {
                importDlls.push_back({dllName, {}});
                dllMap[dllName] = &importDlls.back();
            }
            dllMap[dllName]->thunks.push_back({symName, targetGotRva});
        }
    }

    const auto loadImageSize = static_cast<std::uint32_t>(imageBuf.size());
    const std::uint32_t importSectionRva = Io::AlignUp(loadImageRva + loadImageSize, kSectionAlignment);

    std::vector<std::uint8_t> importSectionData = buildImportSection(importDlls, importSectionRva, gotPatches);
    Io::AlignBuffer(importSectionData, kFileAlignment);

    for (const auto& [gotRva_, iatRva] : gotPatches) {
        const auto destInImage = static_cast<std::uint64_t>(gotRva_);
        if (destInImage + 8 > imageBuf.size())
            throw Domain::RelinkerException("GOT patch target outside image buffer");
        const std::uint64_t iatVa = adjustedBase + iatRva;
        std::memcpy(imageBuf.data() + destInImage, &iatVa, 8);
    }

    const std::uint32_t relocSectionRva = Io::AlignUp(importSectionRva + static_cast<std::uint32_t>(importSectionData.size()), kSectionAlignment);
    std::vector<std::uint8_t> relocSectionData = buildRelocSection(dynamicSection.RelaData, imageBase);
    Io::AlignBuffer(relocSectionData, kFileAlignment);

    const std::uint32_t stubSectionRva = Io::AlignUp(relocSectionRva + static_cast<std::uint32_t>(relocSectionData.size()), kSectionAlignment);
    const std::uint64_t stubVaddr = adjustedBase + stubSectionRva;
    std::vector<std::uint8_t> stubData = buildEntryStub(elfEntryVaddr, stubVaddr);
    Io::AlignBuffer(stubData, kFileAlignment);

    const std::uint32_t entryPointRva = stubSectionRva;

    std::uint16_t numSections = 1;
    if (!importSectionData.empty()) numSections++;
    if (!relocSectionData.empty()) numSections++;
    numSections++;

    const std::uint32_t sizeOfHeaders = Io::AlignUp(
        0x40 + 4 + 20 + 240 + static_cast<std::uint32_t>(numSections) * 40,
        kFileAlignment);

    if (sizeOfHeaders > kSizeOfPeHeaders)
        throw Domain::RelinkerException("PE headers exceed reserved header block size");

    const std::uint32_t sizeOfImage = Io::AlignUp(stubSectionRva + static_cast<std::uint32_t>(stubData.size()), kSectionAlignment);

    const std::uint32_t mzPeOffset = 0x40;
    std::vector<std::uint8_t> result(kSizeOfPeHeaders, 0);

    Io::WriteU16(result, 0, kMzMagic);
    Io::WriteU32(result, 0x3c, mzPeOffset);

    std::size_t off = mzPeOffset;
    Io::WriteU32(result, off, kPeSignature); off += 4;

    Io::WriteU16(result, off, kMachinAmd64); off += 2;
    Io::WriteU16(result, off, numSections); off += 2;
    Io::WriteU32(result, off, 0); off += 4;
    Io::WriteU32(result, off, 0); off += 4;
    Io::WriteU32(result, off, 0); off += 4;
    Io::WriteU16(result, off, 240); off += 2;
    Io::WriteU16(result, off, kCharsExe | kCharsLargeAddressAware); off += 2;

    Io::WriteU16(result, off, kOptMagicPe32Plus); off += 2;
    Io::WriteU8(result, off, 0); off += 1;
    Io::WriteU8(result, off, 0); off += 1;
    Io::WriteU32(result, off, 0); off += 4;
    Io::WriteU32(result, off, 0); off += 4;
    Io::WriteU32(result, off, 0); off += 4;
    Io::WriteU32(result, off, entryPointRva); off += 4;
    Io::WriteU32(result, off, loadImageRva); off += 4;

    Io::WriteU64(result, off, adjustedBase); off += 8;
    Io::WriteU32(result, off, kSectionAlignment); off += 4;
    Io::WriteU32(result, off, kFileAlignment); off += 4;
    Io::WriteU16(result, off, static_cast<std::uint16_t>(kOsVersion)); off += 2;
    Io::WriteU16(result, off, static_cast<std::uint16_t>(kOsVersionMinor)); off += 2;
    Io::WriteU16(result, off, 0); off += 2;
    Io::WriteU16(result, off, 0); off += 2;
    Io::WriteU16(result, off, static_cast<std::uint16_t>(kOsVersion)); off += 2;
    Io::WriteU16(result, off, static_cast<std::uint16_t>(kOsVersionMinor)); off += 2;
    Io::WriteU32(result, off, 0); off += 4;
    Io::WriteU32(result, off, sizeOfImage); off += 4;
    Io::WriteU32(result, off, kSizeOfPeHeaders); off += 4;
    Io::WriteU32(result, off, 0); off += 4;
    Io::WriteU16(result, off, static_cast<std::uint16_t>(kSubsystemConsole)); off += 2;
    Io::WriteU16(result, off, 0); off += 2;
    Io::WriteU64(result, off, 0); off += 8;
    Io::WriteU64(result, off, 0); off += 8;
    Io::WriteU64(result, off, 0); off += 8;
    Io::WriteU64(result, off, 0); off += 8;
    Io::WriteU32(result, off, 0); off += 4;
    Io::WriteU32(result, off, kNumberOfRvaAndSizes); off += 4;

    for (std::uint32_t i = 0; i < kNumberOfRvaAndSizes; ++i) {
        Io::WriteU32(result, off, 0); off += 4;
        Io::WriteU32(result, off, 0); off += 4;
    }

    if (!importSectionData.empty()) {
        Io::WriteU32(result, mzPeOffset + 4 + 20 + 104, importSectionRva);
        Io::WriteU32(result, mzPeOffset + 4 + 20 + 108, static_cast<std::uint32_t>(importDlls.size() * 20));
    }
    if (!relocSectionData.empty()) {
        Io::WriteU32(result, mzPeOffset + 4 + 20 + 168, relocSectionRva);
        Io::WriteU32(result, mzPeOffset + 4 + 20 + 172, static_cast<std::uint32_t>(relocSectionData.size()));
    }

    std::size_t shdrsOff = mzPeOffset + 4 + 20 + 240;

    auto writeSec = [&](const char* name, std::uint32_t virtualSize, std::uint32_t rva, std::uint32_t rawSize, std::uint32_t rawOff, std::uint32_t chars) {
        char n[8]{};
        std::memcpy(n, name, std::min(std::strlen(name), std::size_t(8)));
        std::memcpy(result.data() + shdrsOff, n, 8);
        Io::WriteU32(result, shdrsOff + 8, virtualSize);
        Io::WriteU32(result, shdrsOff + 12, rva);
        Io::WriteU32(result, shdrsOff + 16, rawSize);
        Io::WriteU32(result, shdrsOff + 20, rawOff);
        Io::WriteU32(result, shdrsOff + 24, 0);
        Io::WriteU32(result, shdrsOff + 28, 0);
        Io::WriteU16(result, shdrsOff + 32, 0);
        Io::WriteU16(result, shdrsOff + 34, 0);
        Io::WriteU32(result, shdrsOff + 36, chars);
        shdrsOff += 40;
    };

    const std::uint32_t loadRawSize = Io::AlignUp(loadImageSize, kFileAlignment);
    std::uint32_t curRawOff = kSizeOfPeHeaders;

    writeSec(".load", loadImageSize, loadImageRva, loadRawSize, curRawOff, kSecExec | kSecRW);
    curRawOff += loadRawSize;

    std::uint32_t importRawOff = 0;
    if (!importSectionData.empty()) {
        importRawOff = curRawOff;
        writeSec(".idata", static_cast<std::uint32_t>(importSectionData.size()), importSectionRva,
            static_cast<std::uint32_t>(importSectionData.size()), curRawOff, kSecRW);
        curRawOff += static_cast<std::uint32_t>(importSectionData.size());
    }

    std::uint32_t relocRawOff = 0;
    if (!relocSectionData.empty()) {
        relocRawOff = curRawOff;
        writeSec(".reloc", static_cast<std::uint32_t>(relocSectionData.size()), relocSectionRva,
            static_cast<std::uint32_t>(relocSectionData.size()), curRawOff, kSecDiscard | kSecRO);
        curRawOff += static_cast<std::uint32_t>(relocSectionData.size());
    }

    const std::uint32_t stubRawOff = curRawOff;
    writeSec(".entry", static_cast<std::uint32_t>(stubData.size()), stubSectionRva,
        static_cast<std::uint32_t>(stubData.size()), stubRawOff, kSecExec);

    Io::AlignBuffer(result, kFileAlignment);

    {
        std::vector<std::uint8_t> loadRaw(imageBuf.begin(), imageBuf.end());
        loadRaw.resize(loadRawSize, 0);
        result.insert(result.end(), loadRaw.begin(), loadRaw.end());
    }

    if (!importSectionData.empty())
        result.insert(result.end(), importSectionData.begin(), importSectionData.end());

    if (!relocSectionData.empty())
        result.insert(result.end(), relocSectionData.begin(), relocSectionData.end());

    result.insert(result.end(), stubData.begin(), stubData.end());

    return result;
}

}
