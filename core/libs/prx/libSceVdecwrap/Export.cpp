#include "prx/libSceVdecwrap/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVdecCore/include/Api.hpp"

namespace {

constexpr int ErrorStructSize = static_cast<int>(0x810c0002u);
constexpr int ErrorCodecType = static_cast<int>(0x810c0003u);
constexpr int ErrorReservedField = static_cast<int>(0x810c0004u);
constexpr int ErrorInputMode = static_cast<int>(0x810c0005u);
constexpr int ErrorNullArgument = static_cast<int>(0x810c0007u);
constexpr int ErrorDirectMemory = static_cast<int>(0x810c0008u);
constexpr int ErrorInputBuffer = static_cast<int>(0x810c0009u);
constexpr int ErrorMapAddress = static_cast<int>(0x810c000au);
constexpr int ErrorResetParam = static_cast<int>(0x810c0014u);

constexpr std::uint64_t ConfigSize = 0x28;

template <typename TValue>
bool ReadField(const void* block, std::size_t offset, TValue& value) {
    const int savedErrno = errno;
    const bool copied =
        GuestCopyIn_nid_no_patch(&value, static_cast<const unsigned char*>(block) + offset, sizeof(value));
    errno = savedErrno;
    return copied;
}

bool ReadSize(const void* block, std::uint64_t& size) {
    return ReadField(block, 0x0, size);
}

int CheckConfig(const void* config) {
    std::int32_t codecType = 0;
    if (!ReadField(config, 0x8, codecType)) return 0;
    if (codecType != 1 && codecType != 2 && codecType != 0x1457 && codecType != 0x1a4e) return ErrorCodecType;
    unsigned char fields[0x1c]{};
    if (!ReadField(config, 0xc, fields)) return 0;
    std::uint32_t reserved = 0;
    std::memcpy(&reserved, fields + 0x18, sizeof(reserved));
    if (reserved != 0) return ErrorReservedField;
    return 0;
}

int CheckConfigAndInfo(const void* config, const void* info, std::uint64_t infoSize) {
    std::uint64_t size = 0;
    if (!ReadSize(config, size)) return 0;
    if (size != ConfigSize) return ErrorStructSize;
    if (!ReadSize(info, size)) return 0;
    if (size != infoSize) return ErrorStructSize;
    return CheckConfig(config);
}

int BuildCoreConfig(const void* config, VdecCoreConfigInfo& result) {
    std::uint32_t codecType = 0;
    std::int32_t fields[6]{};
    if (!ReadField(config, 0x8, codecType) || !ReadField(config, 0xc, fields)) return ErrorNullArgument;

    result = {};
    result.codecType = codecType == 2 || codecType == 0x1a4e ? 1u : 0u;
    result.profile = fields[0];
    result.maxLevel = fields[1];
    result.maxDpbFrameCount = fields[4];
    result.maxFrameWidth = fields[2];
    result.maxFrameHeight = fields[3];
    result.horizontalAlign = fields[5];
    return 0;
}

int MapFrameBufferQueryError(int error) {
    const auto raw = static_cast<std::uint32_t>(error);
    if (static_cast<std::uint32_t>(raw + 0x7f3fffffu) < 3) return static_cast<int>(0x810c0001u);
    if (raw == 0x80c0000du) return error;
    if (raw == 0x80c0000cu) return static_cast<int>(0x810c000cu);
    return 0;
}

} // namespace

extern "C" {

int APS5_VABI sceVdecwrapCreateDecoder(const void* config, const void* resource, void* decoder) {
    if (resource == nullptr || config == nullptr || decoder == nullptr) return ErrorNullArgument;
    if (const int error = CheckConfigAndInfo(config, resource, 0x38); error != 0) return error;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecwrapDeleteDecoder(void* decoder) {
    if (decoder == nullptr) return ErrorNullArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecwrapFlushDecodeOutput(void* decoder, void* output) {
    if (decoder == nullptr || output == nullptr) return ErrorNullArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecwrapGetDecodeOutput(void* decoder, void* output) {
    if (decoder == nullptr || output == nullptr) return ErrorNullArgument;
    std::uint64_t size = 0;
    if (ReadSize(output, size) && size != 0x38) return ErrorStructSize;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecwrapMapDirectMemory(void* decoder, const void* memory) {
    if (decoder == nullptr || memory == nullptr) return ErrorNullArgument;
    std::uint64_t value = 0;
    if (ReadSize(memory, value)) {
        if (value != 0x20) return ErrorStructSize;
        if (ReadField(memory, 0x8, value)) {
            if (value == 0) return ErrorDirectMemory;
            if (ReadField(memory, 0x10, value) && value == 0) return ErrorDirectMemory;
        }
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecwrapMapMemory(void* decoder, std::uint64_t argument2, void* argument3) {
    if (argument3 == nullptr) return ErrorMapAddress;
    if (decoder == nullptr || argument2 == 0) return ErrorNullArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecwrapQueryDecoderMemoryInfo(const void* config, void* info) {
    if (config == nullptr || info == nullptr) return ErrorNullArgument;
    if (const int error = CheckConfigAndInfo(config, info, 0x38); error != 0) return error;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecwrapQueryFrameBufferInfo(const void* config, void* info) {
    if (config == nullptr || info == nullptr) return ErrorNullArgument;
    if (const int error = CheckConfigAndInfo(config, info, 0x18); error != 0) return error;

    VdecCoreConfigInfo coreConfig{};
    if (const int error = BuildCoreConfig(config, coreConfig); error != 0) return error;
    VdecCoreFrameBufferInfo coreInfo{};
    const int queryResult = sceVdecCoreQueryFrameBufferInfo(&coreConfig, &coreInfo);
    if (const int error = MapFrameBufferQueryError(queryResult); error != 0) return error;

    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(static_cast<unsigned char*>(info) + sizeof(std::uint64_t),
                                                   &coreInfo, 4 * sizeof(std::uint32_t));
    errno = savedErrno;
    if (!copied) return static_cast<int>(0x810c0001u);
    return 0;
}

int APS5_VABI sceVdecwrapResetDecoder(void* decoder, const void* param) {
    if (decoder == nullptr || param == nullptr) return ErrorNullArgument;
    std::uint64_t size = 0;
    if (ReadSize(param, size)) {
        if (size != 0x10) return ErrorStructSize;
        std::uint32_t value = 0;
        if (ReadField(param, 0x8, value)) {
            if (value != 0) return ErrorResetParam;
            if (ReadField(param, 0xc, value) && value >= 3) return ErrorResetParam;
        }
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecwrapSetDecodeInput(void* decoder, const void* input, void* output) {
    if (decoder == nullptr || input == nullptr || output == nullptr) return ErrorNullArgument;
    std::uint64_t value = 0;
    if (ReadSize(input, value)) {
        if (value != 0x48) return ErrorStructSize;
        if (ReadSize(output, value)) {
            if (value != 0x10) return ErrorStructSize;
            if (ReadField(input, 0x8, value)) {
                if (value == 0) return ErrorInputBuffer;
                if (ReadField(input, 0x10, value)) {
                    if (value == 0) return ErrorInputBuffer;
                    unsigned char tail[0x18]{};
                    std::uint32_t mode = 0;
                    if (ReadField(input, 0x30, tail) && ReadField(input, 0x28, mode) && mode >= 3) return ErrorInputMode;
                }
            }
        }
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecwrapSyncDecode(void* decoder, void* argument2) {
    if (decoder == nullptr || argument2 == nullptr) return ErrorNullArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
