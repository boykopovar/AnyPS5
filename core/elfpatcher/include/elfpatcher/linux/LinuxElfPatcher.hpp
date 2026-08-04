#ifndef ELFPATCHER_LINUX_LINUXELFPATCHER_HPP
#define ELFPATCHER_LINUX_LINUXELFPATCHER_HPP

#include <elfpatcher/IElfPatcher.hpp>
#include <elfpatcher/domain/IEntryStubBuilder.hpp>
#include <elfpatcher/domain/IProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/domain/ISectionHeaderTableBuilder.hpp>
#include <io/IByteWriter.hpp>
#include <memory>

namespace Elfpatcher::Linux {

class LinuxElfPatcher : public IElfPatcher {
public:
    LinuxElfPatcher(
        std::shared_ptr<IEntryStubBuilder> entryStubBuilder,
        std::shared_ptr<IProgramHeaderLayoutBuilder> programHeaderLayoutBuilder,
        std::shared_ptr<ISectionHeaderTableBuilder> sectionHeaderTableBuilder,
        std::shared_ptr<Io::IByteWriter> byteWriter
    );

    std::vector<std::uint8_t> Patch(
        const std::vector<std::uint8_t>& sourceElf,
        const std::vector<Domain::ProgramHeader>& originalHeaders,
        const Domain::SysVDynamicSection& dynamicSection
    ) override;

private:
    std::shared_ptr<IEntryStubBuilder> _entryStubBuilder;
    std::shared_ptr<IProgramHeaderLayoutBuilder> _programHeaderLayoutBuilder;
    std::shared_ptr<ISectionHeaderTableBuilder> _sectionHeaderTableBuilder;
    std::shared_ptr<Io::IByteWriter> _byteWriter;

    void _appendDynEntry(std::vector<std::uint8_t>& buf, std::int64_t tag, std::uint64_t val) const;
};

}

#endif
