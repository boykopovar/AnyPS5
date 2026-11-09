#ifndef RELINKER_DOMAIN_RELINKRESULT_HPP
#define RELINKER_DOMAIN_RELINKRESULT_HPP

#include <relinker/domain/Types.hpp>
#include <vector>

namespace Relinker {

struct RelinkPatch {
    FileByteOffset Offset;
    std::vector<std::uint8_t> Bytes;
};

struct RelinkResult {
    std::vector<CallRegistryEntry> RegistryEntries;
    std::vector<ProgramHeader> OriginalHeaders;
    SysVDynamicSection DynamicSection;
    VirtualAddress OriginalPltGotVaddr;
    std::vector<RelinkPatch> Patches;
};

}

#endif
