#ifndef RELINKER_SRC_VALIDATIONPOLICY_HPP
#define RELINKER_SRC_VALIDATIONPOLICY_HPP

#include <relinker/IValidationPolicy.hpp>
#include <set>

namespace Relinker {

class ValidationPolicy : public IValidationPolicy {
public:
    ValidationPolicy() = default;

    void ValidateSyscallAbsence() override;
    void ValidateRelocationTypeSupported(std::uint32_t RelocationTypeValue, Offset Offset) override;
    void ValidateNidBelongsToLibrary(const std::string& Nid, const std::string& Library) override;
    void ValidateSceStructureSize(Size ExpectedSize, Size ActualSize, Offset Offset) override;
    void ValidateDynamicFieldInterpretable(const std::string& FieldName, Offset Offset) override;
    void ValidateNoSyscallInstructions(const std::vector<std::uint8_t>& CodeSection, Offset CodeOffset) override;

    void RegisterLibraryImport(const std::string& Library);

private:
    std::set<std::string> _importedLibraries;
    std::set<std::uint32_t> _supportedRelocationTypes;

    void _initializeSupportedRelocationTypes();
};

}

#endif // RELINKER_SRC_VALIDATIONPOLICY_HPP
