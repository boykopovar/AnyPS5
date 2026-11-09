#ifndef ELFPATCHER_SECTIONS_SECTIONHEADERTABLEBUILDER_HPP
#define ELFPATCHER_SECTIONS_SECTIONHEADERTABLEBUILDER_HPP

#include <elfpatcher/general/ISectionHeaderTableBuilder.hpp>
#include <io/IByteWriter.hpp>
#include <memory>

namespace Elfpatcher {

class SectionHeaderTableBuilder : public ISectionHeaderTableBuilder {
public:
    explicit SectionHeaderTableBuilder(std::shared_ptr<Io::IByteWriter> byteWriter);

    void WriteTable(
        std::vector<std::uint8_t>& buf,
        const SectionHeaderTableRequest& request
    ) const override;

private:
    std::shared_ptr<Io::IByteWriter> _byteWriter;

    void _writeSectionHeader(
        std::vector<std::uint8_t>& buf,
        std::size_t offset,
        std::uint32_t nameOff,
        std::uint32_t type,
        std::uint64_t flags,
        std::uint64_t addr,
        std::uint64_t fileOffset,
        std::uint64_t size,
        std::uint32_t link,
        std::uint32_t info,
        std::uint64_t align,
        std::uint64_t entSize
    ) const;
};

}

#endif
