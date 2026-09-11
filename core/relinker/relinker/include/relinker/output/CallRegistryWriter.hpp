#ifndef RELINKER_OUTPUT_CALLREGISTRYWRITER_HPP
#define RELINKER_OUTPUT_CALLREGISTRYWRITER_HPP

#include <relinker/domain/ICallRegistryWriter.hpp>

namespace Relinker {

class CallRegistryWriter : public ICallRegistryWriter {
public:
    std::string WriteCallRegistry(const std::vector<CallRegistryEntry>& entries) override;
};

}

#endif
