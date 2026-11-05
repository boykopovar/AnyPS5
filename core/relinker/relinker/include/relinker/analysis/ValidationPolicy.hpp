#ifndef RELINKER_ANALYSIS_VALIDATIONPOLICY_HPP
#define RELINKER_ANALYSIS_VALIDATIONPOLICY_HPP

#include <relinker/domain/IValidationPolicy.hpp>
#include <set>

namespace Relinker {

class ValidationPolicy : public IValidationPolicy {
public:
    ValidationPolicy() = default;

    void ValidateRelocationTypeSupported(std::uint32_t relocationTypeValue, FileByteOffset fileByteOffset) override;

private:
    std::set<std::uint32_t> _supportedRelocationTypes;

    void _initializeSupportedRelocationTypes();
};

}

#endif
