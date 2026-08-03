#ifndef RELINKER_ICALLREGISTRYWRITER_HPP
#define RELINKER_ICALLREGISTRYWRITER_HPP

#include <relinker/Types.hpp>
#include <string>
#include <vector>

namespace Relinker {

struct CallRegistryEntry {
    std::string Nid;
    std::string Library;
    std::string RelocationTypeString;
    Offset RelocationOffset;
    std::string TargetSection;
    Offset TargetOffset;
    std::vector<Offset> CallSites;
    bool CallSitesResolved;
};

class ICallRegistryWriter {
public:
    virtual ~ICallRegistryWriter() = default;

    virtual std::string WriteCallRegistry(const std::vector<CallRegistryEntry>& Entries) = 0;
};

}

#endif // RELINKER_ICALLREGISTRYWRITER_HPP
