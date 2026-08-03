#ifndef RELINKER_IVALIDATIONPOLICY_HPP
#define RELINKER_IVALIDATIONPOLICY_HPP

#include <relinker/Types.hpp>

namespace Relinker {

class IValidationPolicy {
public:
    virtual ~IValidationPolicy() = default;

    virtual void ValidateSyscallAbsence() = 0;
    virtual void ValidateRelocationTypeSupported(std::uint32_t relocationTypeValue, FileByteOffset fileByteOffset) = 0;
    virtual void ValidateNidBelongsToLibrary(const std::string& nid, const std::string& library) = 0;
    virtual void ValidateSceStructureSize(ByteCount expectedSize, ByteCount actualSize, FileByteOffset fileByteOffset) = 0;
    virtual void ValidateDynamicFieldInterpretable(const std::string& fieldName, FileByteOffset fileByteOffset) = 0;
    virtual void ValidateNoSyscallInstructions(const std::vector<std::uint8_t>& codeSection, FileByteOffset codeOffset) = 0;
};

}

#endif // RELINKER_IVALIDATIONPOLICY_HPP
