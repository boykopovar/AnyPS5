#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <elfpatcher/domain/ElfConstants.hpp>
#include <elfpatcher/domain/ProgramHeaderLayoutRequest.hpp>
#include <elfpatcher/domain/SectionHeaderTableRequest.hpp>

namespace Elfpatcher::Linux {

LinuxElfPatcher::LinuxElfPatcher(
    std::shared_ptr<IEntryStubBuilder> entryStubBuilder,
    std::shared_ptr<IProgramHeaderLayoutBuilder> programHeaderLayoutBuilder,
    std::shared_ptr<ISectionHeaderTableBuilder> sectionHeaderTableBuilder,
    std::shared_ptr<Io::IByteWriter> byteWriter
)
    : _entryStubBuilder(std::move(entryStubBuilder))
    , _programHeaderLayoutBuilder(std::move(programHeaderLayoutBuilder))
    , _sectionHeaderTableBuilder(std::move(sectionHeaderTableBuilder))
    , _byteWriter(std::move(byteWriter))
{
}

void LinuxElfPatcher::_appendDynEntry(std::vector<std::uint8_t>& buf, std::int64_t tag, std::uint64_t val) const {
    _byteWriter->AppendI64(buf, tag);
    _byteWriter->AppendU64(buf, val);
}

std::vector<std::uint8_t> LinuxElfPatcher::Patch(
    const std::vector<std::uint8_t>& sourceElf,
    const std::vector<Domain::ProgramHeader>& originalHeaders,
    const Domain::SysVDynamicSection& dynSection)
{
    std::vector<std::uint8_t> buf = sourceElf;

    buf[7] = 0;
    buf[8] = 0;
    buf[0x10] = static_cast<std::uint8_t>(ET_DYN & 0xFF);
    buf[0x11] = static_cast<std::uint8_t>((ET_DYN >> 8) & 0xFF);
    _byteWriter->WriteU64(buf, 40, 0);
    _byteWriter->WriteU16(buf, 60, 0);
    _byteWriter->WriteU16(buf, 62, 0);

    const std::uint64_t phOff = *reinterpret_cast<const std::uint64_t*>(buf.data() + 32);
    const std::uint16_t phEntSize = *reinterpret_cast<const std::uint16_t*>(buf.data() + 54);
    const std::uint16_t phNum = *reinterpret_cast<const std::uint16_t*>(buf.data() + 56);

    const auto alignBuf = [](std::vector<std::uint8_t>& b, std::size_t alignment) {
        while (b.size() % alignment != 0)
            b.push_back(0);
    };

    const auto dynStrOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.DynStrData)
        buf.push_back(b);
    alignBuf(buf, 24);

    const auto dynSymOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.DynSymData)
        buf.push_back(b);
    alignBuf(buf, 24);

    const auto relaOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.RelaData)
        buf.push_back(b);
    alignBuf(buf, 24);

    const auto relaPltOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.RelaPltData)
        buf.push_back(b);
    alignBuf(buf, 8);

    std::vector<std::uint8_t> dynSegBuf;
    for (std::uint8_t b : dynSection.DynamicSegmentData)
        dynSegBuf.push_back(b);
    _appendDynEntry(dynSegBuf, DT_STRTAB, dynStrOff);
    _appendDynEntry(dynSegBuf, DT_STRSZ, dynSection.DynStrData.size());
    _appendDynEntry(dynSegBuf, DT_SYMTAB, dynSymOff);
    _appendDynEntry(dynSegBuf, DT_SYMENT, 24);
    if (!dynSection.RelaData.empty()) {
        _appendDynEntry(dynSegBuf, DT_RELA, relaOff);
        _appendDynEntry(dynSegBuf, DT_RELASZ, dynSection.RelaData.size());
        _appendDynEntry(dynSegBuf, DT_RELAENT, 24);
    }
    if (!dynSection.RelaPltData.empty()) {
        _appendDynEntry(dynSegBuf, DT_JMPREL, relaPltOff);
        _appendDynEntry(dynSegBuf, DT_PLTRELSZ, dynSection.RelaPltData.size());
        _appendDynEntry(dynSegBuf, DT_PLTREL, static_cast<std::uint64_t>(DT_RELA));
    }
    _appendDynEntry(dynSegBuf, DT_NULL, 0);

    const auto dynSegOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSegBuf)
        buf.push_back(b);

    const std::uint64_t realEntryVaddr = *reinterpret_cast<const std::uint64_t*>(buf.data() + 24);
    const auto stubOff = static_cast<std::uint64_t>(buf.size());
    const auto stubBytes = _entryStubBuilder->BuildEntryStub(stubOff, realEntryVaddr);
    for (std::uint8_t b : stubBytes)
        buf.push_back(b);
    _byteWriter->WriteU64(buf, 24, stubOff);

    static constexpr char kInterp[] = "/lib64/ld-linux-x86-64.so.2";
    const auto interpOff = static_cast<std::uint64_t>(buf.size());
    constexpr std::uint64_t interpSize = sizeof(kInterp);
    for (char i : kInterp)
        buf.push_back(static_cast<std::uint8_t>(i));

    const std::uint64_t extraBlockOff = dynStrOff;
    const std::uint64_t extraBlockSize = static_cast<std::uint64_t>(buf.size()) - extraBlockOff;

    ProgramHeaderLayoutRequest layoutRequest{};
    layoutRequest.PhOff = phOff;
    layoutRequest.PhEntSize = phEntSize;
    layoutRequest.PhNum = phNum;
    layoutRequest.OriginalHeaders = originalHeaders;
    layoutRequest.ExtraBlockOffset = extraBlockOff;
    layoutRequest.ExtraBlockSize = extraBlockSize;
    layoutRequest.DynamicSegmentOffset = dynSegOff;
    layoutRequest.DynamicSegmentSize = dynSegBuf.size();
    layoutRequest.InterpOffset = interpOff;
    layoutRequest.InterpSize = interpSize;

    const std::uint16_t writtenPh = _programHeaderLayoutBuilder->WriteLayout(buf, layoutRequest);
    _byteWriter->WriteU16(buf, 56, writtenPh);

    SectionHeaderTableRequest sectionRequest{};
    sectionRequest.DynStrOffset = dynStrOff;
    sectionRequest.DynStrSize = dynSection.DynStrData.size();
    sectionRequest.DynSymOffset = dynSymOff;
    sectionRequest.DynSymSize = dynSection.DynSymData.size();
    sectionRequest.DynamicSegmentOffset = dynSegOff;
    sectionRequest.DynamicSegmentSize = dynSegBuf.size();
    sectionRequest.StubOffset = stubOff;
    sectionRequest.StubSize = stubBytes.size();

    _sectionHeaderTableBuilder->WriteTable(buf, sectionRequest);

    return buf;
}

}
