#ifndef RELINKER_DOMAIN_IRELINKERPIPELINE_HPP
#define RELINKER_DOMAIN_IRELINKERPIPELINE_HPP

#include <relinker/domain/RelinkResult.hpp>
#include <cstdint>
#include <vector>

namespace Relinker {

class IRelinkerPipeline {
public:
    virtual ~IRelinkerPipeline() = default;

    virtual RelinkResult Relink(const std::vector<std::uint8_t>& sourceElf) = 0;
};

}

#endif
