#ifndef ELFPATCHER_LAYOUT_PROGRAMHEADERLAYOUTBUILDER_HPP
#define ELFPATCHER_LAYOUT_PROGRAMHEADERLAYOUTBUILDER_HPP

#include <elfpatcher/general/IProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/ISegmentFilter.hpp>
#include <io/IByteWriter.hpp>
#include <memory>

namespace Elfpatcher {

    class ProgramHeaderLayoutBuilder : public IProgramHeaderLayoutBuilder {
    public:
        ProgramHeaderLayoutBuilder(
            std::shared_ptr<ISegmentFilter> segmentFilter,
            std::shared_ptr<Io::IByteWriter> byteWriter
        );

        std::uint64_t ComputeExtraBlockVaddr(const std::vector<Domain::ProgramHeader>& originalHeaders) const override;

        std::uint16_t WriteLayout(
            std::vector<std::uint8_t>& buf,
            const ProgramHeaderLayoutRequest& request
        ) const override;

    private:
        std::shared_ptr<ISegmentFilter> _segmentFilter;
        std::shared_ptr<Io::IByteWriter> _byteWriter;

        [[nodiscard]] Domain::ProgramHeader _makeLoadHeader(std::uint64_t offset, std::uint64_t vaddr, std::uint64_t size) const;
        [[nodiscard]] Domain::ProgramHeader _makeHeaderBlockLoad(std::uint64_t vaddr, std::uint64_t size, std::uint64_t align) const;
        [[nodiscard]] Domain::ProgramHeader _makePhdrHeader(std::uint64_t offset, std::uint64_t vaddr, std::uint64_t size) const;
        [[nodiscard]] Domain::ProgramHeader _makeDynamicHeader(std::uint64_t offset, std::uint64_t vaddr, std::uint64_t size) const;
        [[nodiscard]] Domain::ProgramHeader _makeInterpHeader(std::uint64_t offset, std::uint64_t vaddr, std::uint64_t size) const;
        [[nodiscard]] std::uint32_t _fixLoadFlags(std::uint32_t originalFlags) const;
        void _writeProgramHeader(std::vector<std::uint8_t>& buf, std::size_t offset, const Domain::ProgramHeader& ph) const;
    };

}

#endif
