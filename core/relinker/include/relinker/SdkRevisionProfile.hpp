#ifndef RELINKER_SDKREVISIONPROFILE_HPP
#define RELINKER_SDKREVISIONPROFILE_HPP

#include <relinker/ISdkRevisionProfile.hpp>
#include <map>

namespace Relinker {

class SdkRevisionProfile : public ISdkRevisionProfile {
public:
    explicit SdkRevisionProfile(const std::vector<std::uint8_t>& SceDynlibData);
    
    std::uint32_t GetSceModuleParamHeaderSize() const override;
    std::uint32_t GetSceModuleParamEntrySize() const override;
    std::uint32_t GetSceDynlibDataHeaderSize() const override;
    std::uint32_t GetSceDynlibDataEntrySize() const override;
    std::uint32_t GetRelocationEntrySize() const override;
    std::uint32_t GetImportsOffset() const override;
    std::uint32_t GetImportsCount(const std::vector<std::uint8_t>& Data) const override;
    std::uint32_t GetRelocationTableOffset() const override;

private:
    std::uint32_t _sdkRevision;
    
    void _detectSdkRevision(const std::vector<std::uint8_t>& Data);
    std::uint32_t _readU32LittleEndian(const std::vector<std::uint8_t>& Data, Offset Offset) const;
    std::uint16_t _readU16LittleEndian(const std::vector<std::uint8_t>& Data, Offset Offset) const;
};

}

#endif // RELINKER_SDKREVISIONPROFILE_HPP
