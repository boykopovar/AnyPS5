#ifndef RELINKER_DOMAIN_ICALLREGISTRYWRITER_HPP
#define RELINKER_DOMAIN_ICALLREGISTRYWRITER_HPP

#include <relinker/domain/Types.hpp>
#include <string>
#include <vector>

namespace Relinker {

struct CallRegistryEntry {
    std::string Nid;
    std::string Library;
    std::string RelocationTypeString;
    FileByteOffset RelocationOffset;
    std::string TargetSection;
    FileByteOffset TargetOffset;
    std::vector<FileByteOffset> CallSites;
    bool CallSitesResolved;
};

class ICallRegistryWriter {
public:
    virtual ~ICallRegistryWriter() = default;

    virtual std::string WriteCallRegistry(const std::vector<CallRegistryEntry>& entries) = 0;
};

}

#endif
