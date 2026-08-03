#ifndef RELINKER_IVALIDATIONPOLICY_HPP
#define RELINKER_IVALIDATIONPOLICY_HPP

#include <relinker/Types.hpp>

namespace Relinker {

class IValidationPolicy {
public:
    virtual ~IValidationPolicy() = default;

    virtual void ValidateSyscallAbsence() = 0;
    virtual void ValidateRelocationTypeSupported(std::uint32_t RelocationTypeValue, Offset Offset) = 0;
    virtual void ValidateNidBelongsToLibrary(const std::string& Nid, const std::string& Library) = 0;
    virtual void ValidateSceStructureSize(Size ExpectedSize, Size ActualSize, Offset Offset) = 0;
    virtual void ValidateDynamicFieldInterpretable(const std::string& FieldName, Offset Offset) = 0;
    virtual void ValidateNoSyscallInstructions(const std::vector<std::uint8_t>& CodeSection, Offset CodeOffset) = 0;
};

}

#endif // RELINKER_IVALIDATIONPOLICY_HPP
