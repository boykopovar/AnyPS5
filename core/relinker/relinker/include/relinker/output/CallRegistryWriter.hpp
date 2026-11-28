#ifndef RELINKER_OUTPUT_CALLREGISTRYWRITER_HPP
#define RELINKER_OUTPUT_CALLREGISTRYWRITER_HPP

#include <relinker/domain/ICallRegistryWriter.hpp>
#include <string_view>

namespace Relinker {

std::string EncodeJsonString(std::string_view value, std::string_view context);

class CallRegistryWriter : public ICallRegistryWriter {
public:
    std::string WriteCallRegistry(const std::vector<CallRegistryEntry>& entries) override;
    std::string WriteModuleImports(const std::vector<CallRegistryEntry>& entries) override;
    std::string WriteNeededLibraries(const std::vector<std::string>& libraries) override;
};

}

#endif
