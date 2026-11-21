#ifndef RELINKER_ANALYSIS_EXCEPTIONTABLEPARSER_HPP
#define RELINKER_ANALYSIS_EXCEPTIONTABLEPARSER_HPP

#include <relinker/domain/Types.hpp>
#include <cstdint>
#include <span>
#include <vector>

namespace Relinker {

struct ActionRecord {
    std::int64_t Filter = 0;
    std::int64_t NextDisplacement = 0;
    VirtualAddress RecordAddress = 0;
    VirtualAddress NextRecordAddress = 0;
};

struct CallSiteDescriptor {
    VirtualAddress Start = 0;
    VirtualAddress Length = 0;
    VirtualAddress LandingPad = 0;
    std::uint64_t ActionOffset = 0;
    std::vector<ActionRecord> ActionChain;
    bool LinksToCatchAll = false;
};

struct ParsedExceptionTable {
    VirtualAddress LandingBase = 0;
    std::uint8_t LandingEncoding = 0xFF;
    std::uint8_t TypeEncoding = 0xFF;
    VirtualAddress TypeTable = 0;
    std::uint8_t CallEncoding = 0;
    VirtualAddress CallSiteTable = 0;
    VirtualAddress ActionTable = 0;
    std::vector<CallSiteDescriptor> CallSites;
};

class ExceptionTableParser {
public:
    ParsedExceptionTable Parse(std::span<const std::uint8_t> bytes, VirtualAddress lsdaOffset, VirtualAddress functionBegin);
    ParsedExceptionTable Parse(const std::vector<std::uint8_t>& bytes, const std::vector<ProgramHeader>& headers, VirtualAddress lsdaAddress, VirtualAddress functionBegin);
};

}

#endif
