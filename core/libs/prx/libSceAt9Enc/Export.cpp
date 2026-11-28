#include "prx/libSceAt9Enc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <array>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int32_t SCE_AT9ENC_ERROR_INVALID_ARGUMENT = static_cast<int32_t>(0x81210001u);
constexpr int32_t SCE_AT9ENC_ERROR_INVALID_PARAMETER = static_cast<int32_t>(0x81210000u);

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

bool CopyIn(void* destination, const void* source, std::size_t size) {
    ErrnoGuard guard;
    return GuestCopyIn_nid_no_patch(destination, source, size);
}

bool CopyOut(void* destination, const void* source, std::size_t size) {
    ErrnoGuard guard;
    return GuestCopyOut_nid_no_patch(destination, source, size);
}

bool IsSupportedBitrate(uint32_t mode, uint32_t bitrate) {
    switch (mode) {
    case 1: return bitrate == 0x24 || bitrate == 0x30 || bitrate == 0x3c || bitrate == 0x48 || bitrate == 0x54 || bitrate == 0x60 || bitrate == 0x78 || bitrate == 0x90;
    case 2: return bitrate == 0xc0 || bitrate == 0xf0 || bitrate == 0x120 || bitrate == 0x180;
    case 4: return bitrate == 0x48 || bitrate == 0x60 || bitrate == 0x78 || bitrate == 0x90 || bitrate == 0xa8 || bitrate == 0xc0 || bitrate == 0xf0 || bitrate == 0x120;
    case 6: return bitrate == 0xf0 || bitrate == 0x12c || bitrate == 0x168 || bitrate == 0x1e0;
    case 8: return bitrate == 0x150 || bitrate == 0x1a4 || bitrate == 0x1f8 || bitrate == 0x2a0;
    default: return false;
    }
}

uint32_t QueryMemorySize(const std::array<uint32_t, 13>& parameters) {
    const uint32_t mode = parameters[1];
    const uint32_t coefficient = mode == 1 ? 1 : mode == 2 ? 2 : mode == 4 ? 2 : mode == 6 ? 4 : 5;
    const uint32_t internalMemory = 0xf0 + coefficient * 0x220 + mode * (0xd18 + 0x1820) + 4;
    const uint32_t shift = parameters[4] == 2 ? 9 : 10;
    return 0x10060 + ((parameters[5] * mode) << shift) + internalMemory;
}

bool IsValidParameters(const std::array<uint32_t, 13>& parameters) {
    if (parameters[0] != 0x34) return false;
    const uint32_t mode = parameters[1];
    if (mode != 1 && mode != 2 && mode != 4 && mode != 6 && mode != 8) return false;
    if (!IsSupportedBitrate(mode, parameters[3])) return false;
    if (parameters[2] != 0x2ee0 && parameters[2] != 0x5dc0 && parameters[2] != 0xbb80) return false;
    if (parameters[4] != 2 && parameters[4] != 4) return false;
    if (parameters[5] != 1 && parameters[5] != 4) return false;
    if (parameters[6] > 1 || parameters[7] > 1 || parameters[11] > 2) return false;
    return true;
}

bool IsMisaligned(const void* pointer) {
    return (reinterpret_cast<std::uintptr_t>(pointer) & 7) != 0;
}

} // namespace

extern "C" {

int32_t APS5_VABI sceAt9EncClearContext(void *pAt9EncWorkArea, int32_t *pInternalResult) {
    if (pAt9EncWorkArea == nullptr || pInternalResult == nullptr || IsMisaligned(pAt9EncWorkArea)) {
        return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceAt9EncCreateEncoder(void *pAt9EncWorkArea, void *pAt9EncParam, int32_t memSize, int32_t *pInternalResult) {
    if (IsMisaligned(pAt9EncWorkArea) || pAt9EncWorkArea == nullptr || pAt9EncParam == nullptr || pInternalResult == nullptr) {
        return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceAt9EncEncode(void *pAt9EncWorkArea, const void *pInputBuf, void *pOutputBuf, uint32_t inputBufSize, uint32_t outputBufSize, uint32_t *pOutputDataByte, uint32_t *pUsedBufSize, int32_t *pInternalResult) {
    if (IsMisaligned(pAt9EncWorkArea) || pAt9EncWorkArea == nullptr || pInputBuf == nullptr || pOutputBuf == nullptr ||
        pOutputDataByte == nullptr || pUsedBufSize == nullptr) {
        return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceAt9EncFlush(void *pAt9EncWorkArea, void *pOutputBuf, uint32_t outputBufSize, uint32_t *pOutputDataByte, int32_t *pInternalResult) {
    if (IsMisaligned(pAt9EncWorkArea) || pAt9EncWorkArea == nullptr || pOutputBuf == nullptr || pOutputDataByte == nullptr ||
        pInternalResult == nullptr) {
        return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceAt9EncQueryMemSize(void *pAt9EncParam, int32_t *pMemSize, int32_t *pInternalResult) {
    if (pAt9EncParam == nullptr || pMemSize == nullptr || pInternalResult == nullptr) return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    const int32_t zero = 0;
    if (!CopyOut(pMemSize, &zero, sizeof(zero)) || !CopyOut(pInternalResult, &zero, sizeof(zero))) return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    std::array<uint32_t, 13> parameters{};
    if (!CopyIn(parameters.data(), pAt9EncParam, sizeof(parameters))) return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    if (!IsValidParameters(parameters)) {
        const int32_t internalError = -999;
        if (!CopyOut(pInternalResult, &internalError, sizeof(internalError))) return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
        return SCE_AT9ENC_ERROR_INVALID_PARAMETER;
    }
    const int32_t memorySize = static_cast<int32_t>(QueryMemorySize(parameters));
    if (!CopyOut(pMemSize, &memorySize, sizeof(memorySize))) return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    return 0;
}

}
