#ifndef RELINKER_PARSING_SDKREVISIONPROFILE_HPP
#define RELINKER_PARSING_SDKREVISIONPROFILE_HPP

#include <relinker/domain/ISdkRevisionProfile.hpp>
#include <map>

namespace Relinker {

class SdkRevisionProfile : public ISdkRevisionProfile {
public:
    explicit SdkRevisionProfile(const std::vector<std::uint8_t>& sceDynlibData);
    
    [[nodiscard]] std::uint32_t GetSceModuleParamHeaderSize() const override;
    [[nodiscard]] std::uint32_t GetSceModuleParamEntrySize() const override;
    [[nodiscard]] std::uint32_t GetSceDynlibDataHeaderSize() const override;
    [[nodiscard]] std::uint32_t GetSceDynlibDataEntrySize() const override;
    [[nodiscard]] std::uint32_t GetRelocationEntrySize() const override;
    [[nodiscard]] std::uint32_t GetImportsOffset() const override;
    [[nodiscard]] std::uint32_t GetImportsCount(const std::vector<std::uint8_t>& data) const override;
    [[nodiscard]] std::uint32_t GetRelocationTableOffset() const override;

private:
    std::uint32_t _sdkRevision;
    
    void _detectSdkRevision(const std::vector<std::uint8_t>& data);
    [[nodiscard]] std::uint32_t _readU32LittleEndian(const std::vector<std::uint8_t>& data, FileByteOffset fileByteOffset) const;
    [[nodiscard]] std::uint16_t _readU16LittleEndian(const std::vector<std::uint8_t>& data, FileByteOffset fileByteOffset) const;
};

}

#endif // RELINKER_SDKREVISIONPROFILE_HPP
