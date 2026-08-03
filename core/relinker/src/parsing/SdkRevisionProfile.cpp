#include <relinker/parsing/SdkRevisionProfile.hpp>
#include <cstring>

namespace Relinker {

constexpr std::uint32_t DEFAULT_SDK_REVISION = 0x05000000;

constexpr std::uint32_t SCE_MODULE_PARAM_HEADER_SIZE = 0x38;
constexpr std::uint32_t SCE_MODULE_PARAM_ENTRY_SIZE = 0x08;
constexpr std::uint32_t SCE_DYNLIBDATA_HEADER_SIZE = 0x10;
constexpr std::uint32_t SCE_DYNLIBDATA_ENTRY_SIZE = 0x10;
constexpr std::uint32_t RELOCATION_ENTRY_SIZE = 0x18;

SdkRevisionProfile::SdkRevisionProfile(const std::vector<std::uint8_t>& sceDynlibData)
    : _sdkRevision(DEFAULT_SDK_REVISION) {
    _detectSdkRevision(sceDynlibData);
}

void SdkRevisionProfile::_detectSdkRevision(const std::vector<std::uint8_t>& data) {
    if (data.size() < 8) {
        return;
    }
    
    _sdkRevision = _readU32LittleEndian(data, 0x04);
}

std::uint32_t SdkRevisionProfile::_readU32LittleEndian(
    const std::vector<std::uint8_t>& data, const FileByteOffset fileByteOffset) const {
    if (fileByteOffset + 4 > data.size()) {
        throw RelinkerException("FileByteOffset out of bounds", fileByteOffset);
    }
    std::uint32_t value;
    std::memcpy(&value, data.data() + fileByteOffset, 4);
    return value;
}

std::uint16_t SdkRevisionProfile::_readU16LittleEndian(
    const std::vector<std::uint8_t>& data, const FileByteOffset fileByteOffset) const {
    if (fileByteOffset + 2 > data.size()) {
        throw RelinkerException("FileByteOffset out of bounds", fileByteOffset);
    }
    std::uint16_t value;
    std::memcpy(&value, data.data() + fileByteOffset, 2);
    return value;
}

std::uint32_t SdkRevisionProfile::GetSceModuleParamHeaderSize() const {
    return SCE_MODULE_PARAM_HEADER_SIZE;
}

std::uint32_t SdkRevisionProfile::GetSceModuleParamEntrySize() const {
    return SCE_MODULE_PARAM_ENTRY_SIZE;
}

std::uint32_t SdkRevisionProfile::GetSceDynlibDataHeaderSize() const {
    return SCE_DYNLIBDATA_HEADER_SIZE;
}

std::uint32_t SdkRevisionProfile::GetSceDynlibDataEntrySize() const {
    return SCE_DYNLIBDATA_ENTRY_SIZE;
}

std::uint32_t SdkRevisionProfile::GetRelocationEntrySize() const {
    return RELOCATION_ENTRY_SIZE;
}

std::uint32_t SdkRevisionProfile::GetImportsOffset() const {
    return SCE_DYNLIBDATA_HEADER_SIZE;
}

std::uint32_t SdkRevisionProfile::GetImportsCount(const std::vector<std::uint8_t>& data) const {
    if (data.size() < 8) {
        return 0;
    }
    return _readU32LittleEndian(data, 0x00);
}

std::uint32_t SdkRevisionProfile::GetRelocationTableOffset() const {
    return 0;
}

}
