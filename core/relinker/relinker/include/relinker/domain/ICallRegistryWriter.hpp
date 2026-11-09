#ifndef RELINKER_DOMAIN_ICALLREGISTRYWRITER_HPP
#define RELINKER_DOMAIN_ICALLREGISTRYWRITER_HPP

#include <relinker/domain/Types.hpp>
#include <string>
#include <vector>

namespace Relinker {

class ICallRegistryWriter {
public:
    virtual ~ICallRegistryWriter() = default;

    virtual std::string WriteCallRegistry(const std::vector<CallRegistryEntry>& entries) = 0;
    virtual std::string WriteModuleImports(const std::vector<CallRegistryEntry>& entries) = 0;
    virtual std::string WriteNeededLibraries(const std::vector<std::string>& libraries) = 0;
};

}

#endif
