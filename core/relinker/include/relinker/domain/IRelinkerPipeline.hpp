#ifndef RELINKER_DOMAIN_IRELINKERPIPELINE_HPP
#define RELINKER_DOMAIN_IRELINKERPIPELINE_HPP

#include <string>

namespace Relinker {

class IRelinkerPipeline {
public:
    virtual ~IRelinkerPipeline() = default;

    virtual void Run(
        const std::string& inputElfPath,
        const std::string& outputRegistryPath,
        const std::string& outputElfPath
    ) = 0;
};

}

#endif
