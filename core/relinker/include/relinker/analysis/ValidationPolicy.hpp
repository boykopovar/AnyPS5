#ifndef RELINKER_ANALYSIS_VALIDATIONPOLICY_HPP
#define RELINKER_ANALYSIS_VALIDATIONPOLICY_HPP

#include <relinker/domain/IValidationPolicy.hpp>
#include <set>

namespace Relinker {

class ValidationPolicy : public IValidationPolicy {
public:
    ValidationPolicy() = default;

    void ValidateSyscallAbsence() override;
    void ValidateRelocationTypeSupported(std::uint32_t relocationTypeValue, FileByteOffset fileByteOffset) override;
    void ValidateNidBelongsToLibrary(const std::string& nid, const std::string& library) override;
    void ValidateSceStructureSize(ByteCount expectedSize, ByteCount actualSize, FileByteOffset fileByteOffset) override;
    void ValidateDynamicFieldInterpretable(const std::string& fieldName, FileByteOffset fileByteOffset) override;
    void ValidateNoSyscallInstructions(const std::vector<std::uint8_t>& codeSection, FileByteOffset codeOffset) override;

    void RegisterLibraryImport(const std::string& library);

private:
    std::set<std::string> _importedLibraries;
    std::set<std::uint32_t> _supportedRelocationTypes;

    void _initializeSupportedRelocationTypes();
};

}

#endif
