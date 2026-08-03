#ifndef RELINKER_IELFREADER_HPP
#define RELINKER_IELFREADER_HPP

#include <relinker/Types.hpp>
#include <vector>
#include <memory>

namespace Relinker {

class IElfReader {
public:
    virtual ~IElfReader() = default;

    [[nodiscard]] virtual ElfHeader ReadHeader() const = 0;
    [[nodiscard]] virtual std::vector<ProgramHeader> ReadProgramHeaders() const = 0;
    [[nodiscard]] virtual std::vector<SectionHeader> ReadSectionHeaders() const = 0;
    [[nodiscard]] virtual std::vector<DynamicTag> ReadDynamicTags(const ProgramHeader& dynamicHeader) const = 0;
    [[nodiscard]] virtual FileByteOffset TranslateVirtualAddress(VirtualAddress address) const = 0;
    [[nodiscard]] virtual std::vector<std::uint8_t> ReadSection(const SectionHeader& header) const = 0;
    [[nodiscard]] virtual std::vector<std::uint8_t> ReadSegment(const ProgramHeader& header) const = 0;
    [[nodiscard]] virtual std::uint64_t GetFileSize() const = 0;
};

}

#endif // RELINKER_IELFREADER_HPP
