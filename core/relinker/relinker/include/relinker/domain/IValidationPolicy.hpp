#ifndef RELINKER_DOMAIN_IVALIDATIONPOLICY_HPP
#define RELINKER_DOMAIN_IVALIDATIONPOLICY_HPP

#include <relinker/domain/Types.hpp>

namespace Relinker {

class IValidationPolicy {
public:
    virtual ~IValidationPolicy() = default;

    virtual void ValidateRelocationTypeSupported(std::uint32_t relocationTypeValue, FileByteOffset fileByteOffset) = 0;
};

}

#endif
