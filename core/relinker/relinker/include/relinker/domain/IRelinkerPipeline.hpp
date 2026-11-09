#ifndef RELINKER_DOMAIN_IRELINKERPIPELINE_HPP
#define RELINKER_DOMAIN_IRELINKERPIPELINE_HPP

#include <relinker/domain/RelinkResult.hpp>

namespace Relinker {

class IRelinkerPipeline {
public:
    virtual ~IRelinkerPipeline() = default;

    virtual RelinkResult Relink() = 0;
};

}

#endif
