#ifndef RELINKER_IELFREADER_HPP
#define RELINKER_IELFREADER_HPP

#include <relinker/Types.hpp>
#include <vector>
#include <memory>

namespace Relinker {

class IElfReader {
public:
    virtual ~IElfReader() = default;

    virtual ElfHeader ReadHeader() const = 0;
    virtual std::vector<ProgramHeader> ReadProgramHeaders() const = 0;
    virtual std::vector<SectionHeader> ReadSectionHeaders() const = 0;
    virtual std::vector<std::uint8_t> ReadSection(const SectionHeader& Header) const = 0;
    virtual std::vector<std::uint8_t> ReadSegment(const ProgramHeader& Header) const = 0;
    virtual std::uint64_t GetFileSize() const = 0;
};

}

#endif // RELINKER_IELFREADER_HPP
