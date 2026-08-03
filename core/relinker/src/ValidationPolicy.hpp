#ifndef RELINKER_SRC_VALIDATIONPOLICY_HPP
#define RELINKER_SRC_VALIDATIONPOLICY_HPP

#include <relinker/IValidationPolicy.hpp>
#include <set>

namespace Relinker {

class ValidationPolicy : public IValidationPolicy {
public:
    ValidationPolicy() = default;

    void ValidateSyscallAbsence() override;
    void ValidateRelocationTypeSupported(std::uint32_t RelocationTypeValue, FileByteOffset FileByteOffset) override;
    void ValidateNidBelongsToLibrary(const std::string& Nid, const std::string& Library) override;
    void ValidateSceStructureSize(ByteCount ExpectedSize, ByteCount ActualSize, FileByteOffset FileByteOffset) override;
    void ValidateDynamicFieldInterpretable(const std::string& FieldName, FileByteOffset FileByteOffset) override;
    void ValidateNoSyscallInstructions(const std::vector<std::uint8_t>& CodeSection, FileByteOffset CodeOffset) override;

    void RegisterLibraryImport(const std::string& Library);

private:
    std::set<std::string> _importedLibraries;
    std::set<std::uint32_t> _supportedRelocationTypes;

    void _initializeSupportedRelocationTypes();
};

}

#endif // RELINKER_SRC_VALIDATIONPOLICY_HPP
